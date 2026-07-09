#pragma once

// Opportunistic background NTP resync on wake.
//
// The ESP32-C3 has no battery-backed RTC: after deep sleep the clock is restored
// from the drift-prone internal RC timer (g_clockApproximate). When the last NTP
// sync is stale, this module silently connects to the last-used WiFi network in a
// short-lived background task, syncs time, and switches the radio back off.
// Zero impact on sleep current — it only runs while the user woke the device anyway.
namespace OpportunisticTimeSync {

// Call once at boot, after clock restore + TZ setup and once the device has
// committed to staying awake. Spawns the background task only when a sync is
// warranted (approximate clock, stale sync, saved network); no-op otherwise.
void maybeStart();

// Foreground code that is about to take over the WiFi radio (activities doing
// their own connect) calls this so the background task aborts without touching
// WiFi state. Safe to call at any time, including when no task is running.
void claimForeground();

}  // namespace OpportunisticTimeSync
