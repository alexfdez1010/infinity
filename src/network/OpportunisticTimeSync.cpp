#include "OpportunisticTimeSync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Logging.h>

#include <atomic>
#include <cstdlib>
#include <ctime>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"

// Defined in main.cpp
extern bool g_clockApproximate;
extern uint32_t g_lastNtpSyncUnix;

namespace {
constexpr int CONNECT_TIMEOUT_DS = 100;             // 10 s in 100 ms steps, per network
constexpr int SYNC_TIMEOUT_DS = 150;                // 15 s in 100 ms steps
constexpr int MAX_NETWORK_ATTEMPTS = 3;

std::atomic<bool> taskActive{false};
std::atomic<bool> foregroundClaimed{false};
// cancel() sets this when a book is opened mid-sync: like foregroundClaimed it
// stops the task ASAP, but unlike it the task still tears the radio down (no
// foreground consumer is waiting for WiFi — we just want the heap/CPU back).
std::atomic<bool> cancelRequested{false};

// True when the sync should stop early for any reason.
inline bool aborted() { return foregroundClaimed || cancelRequested; }

// Connect to `cred` and wait for association. Returns true when connected.
bool connectTo(const WifiCredential& cred) {
  LOG_DBG("TSYNC", "Background NTP sync: connecting to %s", cred.ssid.c_str());
  WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
  for (int i = 0; i < CONNECT_TIMEOUT_DS && !aborted(); i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return WiFi.status() == WL_CONNECTED;
}

void syncTask(void*) {
  bool synced = false;
  bool connected = false;

  WiFi.mode(WIFI_STA);

  // Scan and try saved networks strongest-first (scan results come RSSI-sorted).
  // Falls back to a blind connect to the last-used network below — hidden SSIDs
  // don't show up in a scan.
  const int found = aborted() ? 0 : WiFi.scanNetworks();
  int attempts = 0;
  for (int i = 0; i < found && !connected && !aborted() && attempts < MAX_NETWORK_ATTEMPTS; i++) {
    const auto* cred = WIFI_STORE.findCredential(WiFi.SSID(i).c_str());
    if (!cred) continue;
    attempts++;
    connected = connectTo(*cred);
    if (!connected) WiFi.disconnect(false);
  }
  WiFi.scanDelete();

  if (!connected && attempts == 0 && !aborted()) {
    const auto& ssid = WIFI_STORE.getLastConnectedSsid();
    const auto* cred = ssid.empty() ? nullptr : WIFI_STORE.findCredential(ssid);
    if (cred) connected = connectTo(*cred);
  }

  if (connected && !aborted()) {
    // configTzTime starts SNTP; onNtpSyncComplete (main.cpp) clears
    // g_clockApproximate and stamps g_lastNtpSyncUnix when the reply lands
    const char* tz = getenv("TZ");
    configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
    for (int i = 0; i < SYNC_TIMEOUT_DS && !aborted(); i++) {
      if (!g_clockApproximate) {
        synced = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  // Tear the radio down unless a foreground activity took it over meanwhile.
  // A cancel (book opened) does NOT claim the radio, so it falls through here and
  // the WiFi/heap is released — exactly what we want so reading isn't disrupted.
  if (!foregroundClaimed) {
    WiFi.disconnect(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.mode(WIFI_OFF);
  }

  LOG_DBG("TSYNC", "Background NTP sync %s%s", synced ? "succeeded" : "gave up",
          foregroundClaimed ? " (foreground claimed WiFi)" : cancelRequested ? " (cancelled)" : "");
  taskActive = false;
  vTaskDelete(nullptr);
}
}  // namespace

namespace OpportunisticTimeSync {

void maybeStart() {
  if (taskActive) return;
  if (!g_clockApproximate) return;  // clock already NTP-accurate this boot
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_MANUAL) return;  // user manages time by hand
  if (WiFi.status() == WL_CONNECTED) return;

  // No epoch-based rate limit on purpose: it would measure staleness with the very
  // clock we distrust (a clock hours behind reports the last sync as "recent" and
  // skips the fix). g_clockApproximate already caps this at one successful sync per
  // boot, and the 15-min retry in main loop only fires while still unsynced.

  WIFI_STORE.loadFromFile();
  if (WIFI_STORE.getCredentials().empty()) return;

  foregroundClaimed = false;
  cancelRequested = false;
  taskActive = true;
  if (xTaskCreate(syncTask, "ntpsync", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    taskActive = false;
    LOG_ERR("TSYNC", "Failed to create background NTP sync task");
  }
}

void claimForeground() { foregroundClaimed = true; }

void cancel() { if (taskActive) cancelRequested = true; }

}  // namespace OpportunisticTimeSync
