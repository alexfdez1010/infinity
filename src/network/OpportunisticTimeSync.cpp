#include "OpportunisticTimeSync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Logging.h>

#include <atomic>
#include <cstdlib>
#include <ctime>

#include "WifiCredentialStore.h"

// Defined in main.cpp
extern bool g_clockApproximate;
extern uint32_t g_lastNtpSyncUnix;

namespace {
constexpr uint32_t STALE_AFTER_SECS = 12UL * 3600;  // resync at most twice a day
constexpr int CONNECT_TIMEOUT_DS = 150;             // 15 s in 100 ms steps
constexpr int SYNC_TIMEOUT_DS = 150;                // 15 s in 100 ms steps

std::atomic<bool> taskActive{false};
std::atomic<bool> foregroundClaimed{false};

void syncTask(void*) {
  const auto& ssid = WIFI_STORE.getLastConnectedSsid();
  const auto* cred = ssid.empty() ? nullptr : WIFI_STORE.findCredential(ssid);
  bool synced = false;

  if (cred && !foregroundClaimed) {
    LOG_DBG("TSYNC", "Background NTP sync: connecting to %s", cred->ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str());

    for (int i = 0; i < CONNECT_TIMEOUT_DS && !foregroundClaimed; i++) {
      if (WiFi.status() == WL_CONNECTED) break;
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (WiFi.status() == WL_CONNECTED && !foregroundClaimed) {
      // configTzTime starts SNTP; onNtpSyncComplete (main.cpp) clears
      // g_clockApproximate and stamps g_lastNtpSyncUnix when the reply lands
      const char* tz = getenv("TZ");
      configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
      for (int i = 0; i < SYNC_TIMEOUT_DS && !foregroundClaimed; i++) {
        if (!g_clockApproximate) {
          synced = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }

  // Tear the radio down only if no foreground activity took it over meanwhile
  if (!foregroundClaimed) {
    WiFi.disconnect(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.mode(WIFI_OFF);
  }

  LOG_DBG("TSYNC", "Background NTP sync %s%s", synced ? "succeeded" : "gave up",
          foregroundClaimed ? " (foreground claimed WiFi)" : "");
  taskActive = false;
  vTaskDelete(nullptr);
}
}  // namespace

namespace OpportunisticTimeSync {

void maybeStart() {
  if (taskActive) return;
  if (!g_clockApproximate) return;  // clock already NTP-accurate this boot
  if (WiFi.status() == WL_CONNECTED) return;

  // Rate limit: skip when the last sync is recent. g_lastNtpSyncUnix lives in
  // RTC memory — if it was lost (cold boot), 0 counts as stale and we just sync.
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (g_lastNtpSyncUnix != 0 && now >= g_lastNtpSyncUnix &&
      now - g_lastNtpSyncUnix < STALE_AFTER_SECS) {
    return;
  }

  WIFI_STORE.loadFromFile();
  if (WIFI_STORE.getLastConnectedSsid().empty()) return;

  foregroundClaimed = false;
  taskActive = true;
  if (xTaskCreate(syncTask, "ntpsync", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    taskActive = false;
    LOG_ERR("TSYNC", "Failed to create background NTP sync task");
  }
}

void claimForeground() { foregroundClaimed = true; }

}  // namespace OpportunisticTimeSync
