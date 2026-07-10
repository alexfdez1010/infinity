#include "OpportunisticTimeSync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Logging.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"

// Defined in main.cpp
extern bool g_clockApproximate;
extern uint32_t g_lastNtpSyncUnix;

namespace {
constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;  // per network
constexpr uint32_t SYNC_TIMEOUT_MS = 15000;     // wait for the SNTP reply
constexpr int MAX_NETWORK_ATTEMPTS = 3;
constexpr uint32_t MIN_FREE_HEAP = 55000;

// State machine, driven entirely from the main loop via poll(). WiFi is brought
// up on the main Arduino task on purpose: WiFi.mode(WIFI_STA) blocks on the
// WiFi-start event, and when called from a separate low-priority FreeRTOS task
// at tight heap that event was never delivered — the device hard-froze (no
// reboot). Every other working WiFi user (Home, Weather, OTA) runs on the main
// task, so we do too. poll() only advances a small slice per iteration, so the
// UI never blocks for more than one WiFi.status() check.
enum class State { IDLE, STARTING, CONNECTING, SYNCING, TEARDOWN };

State state = State::IDLE;
bool foregroundClaimed = false;
std::vector<std::string> ssidQueue;  // networks left to try, in order
size_t queueIdx = 0;
uint32_t phaseStart = 0;  // millis() when the current connect/sync phase began

void teardownRadio() {
  if (!foregroundClaimed) {
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
  }
}

// Begin connecting to ssidQueue[queueIdx]; returns false if the credential
// vanished (skip it).
bool beginCurrent() {
  const auto* cred = WIFI_STORE.findCredential(ssidQueue[queueIdx]);
  if (!cred) return false;
  LOG_INF("TSYNC", "Connecting to %s (heap=%u)", cred->ssid.c_str(), ESP.getFreeHeap());
  WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  phaseStart = millis();
  return true;
}
}  // namespace

namespace OpportunisticTimeSync {

void maybeStart() {
  if (state != State::IDLE) return;             // already running
  if (!g_clockApproximate) return;              // clock already NTP-accurate this boot
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_MANUAL) return;  // user manages time by hand
  if (WiFi.status() == WL_CONNECTED) return;

  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < MIN_FREE_HEAP) {
    LOG_ERR("TSYNC", "Skipping NTP sync: only %u B free heap (< %u)", freeHeap, MIN_FREE_HEAP);
    return;
  }

  WIFI_STORE.loadFromFile();
  if (WIFI_STORE.getCredentials().empty()) return;

  // Build the try-queue: last-used network first, then the rest, capped. No
  // WiFi.scanNetworks() — the scan's AP-list buffer spiked heap and reaching
  // hidden SSIDs needs a blind begin() anyway.
  ssidQueue.clear();
  const auto& last = WIFI_STORE.getLastConnectedSsid();
  if (!last.empty() && WIFI_STORE.findCredential(last)) ssidQueue.push_back(last);
  for (const auto& cred : WIFI_STORE.getCredentials()) {
    if ((int)ssidQueue.size() >= MAX_NETWORK_ATTEMPTS) break;
    if (cred.ssid == last) continue;
    ssidQueue.push_back(cred.ssid);
  }
  if (ssidQueue.empty()) return;

  queueIdx = 0;
  foregroundClaimed = false;
  state = State::STARTING;
}

void poll() {
  switch (state) {
    case State::IDLE:
      return;

    case State::STARTING:
      // Must be at full CPU clock here (caller restores it) or the radio wedges.
      WiFi.mode(WIFI_STA);  // one-time ~200-500ms hitch on the main task (safe here)
      if (!beginCurrent()) {
        // Credential disappeared; try the next, or give up.
        if (++queueIdx >= ssidQueue.size()) { state = State::TEARDOWN; return; }
        return;  // retry beginCurrent next poll
      }
      state = State::CONNECTING;
      return;

    case State::CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        // configTzTime starts SNTP; onNtpSyncComplete (main.cpp) clears
        // g_clockApproximate and stamps g_lastNtpSyncUnix when the reply lands.
        const char* tz = getenv("TZ");
        configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
        phaseStart = millis();
        state = State::SYNCING;
        return;
      }
      if (millis() - phaseStart >= CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(false);
        if (++queueIdx >= ssidQueue.size()) {
          LOG_INF("TSYNC", "NTP sync gave up: no network connected");
          state = State::TEARDOWN;
          return;
        }
        if (!beginCurrent()) { state = State::TEARDOWN; }  // next credential gone
      }
      return;

    case State::SYNCING:
      if (!g_clockApproximate) {
        LOG_INF("TSYNC", "NTP sync succeeded");
        state = State::TEARDOWN;
        return;
      }
      if (millis() - phaseStart >= SYNC_TIMEOUT_MS) {
        LOG_INF("TSYNC", "NTP sync timed out waiting for reply");
        state = State::TEARDOWN;
      }
      return;

    case State::TEARDOWN:
      teardownRadio();
      ssidQueue.clear();
      queueIdx = 0;
      state = State::IDLE;
      return;
  }
}

bool busy() { return state != State::IDLE; }

void claimForeground() {
  // A foreground activity is taking over the radio. Stop advancing and leave
  // WiFi exactly as-is for it — do NOT tear the radio down.
  foregroundClaimed = true;
  ssidQueue.clear();
  queueIdx = 0;
  state = State::IDLE;
}

void cancel() {
  // Abort and release the radio (e.g. a book was opened). Tear WiFi down so the
  // reader isn't starved, unless a foreground consumer claimed it.
  if (state == State::IDLE) return;
  teardownRadio();
  ssidQueue.clear();
  queueIdx = 0;
  state = State::IDLE;
}

}  // namespace OpportunisticTimeSync
