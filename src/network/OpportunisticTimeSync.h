#pragma once

// Opportunistic background NTP resync on wake.
//
// The ESP32-C3 has no battery-backed RTC: after deep sleep the clock is restored
// from the drift-prone internal RC timer (g_clockApproximate). When the last NTP
// sync is stale, this module silently scans for saved WiFi networks (falling back
// to a blind connect to the last-used one, for hidden SSIDs) in a short-lived
// background task, syncs time, and switches the radio back off. Skipped entirely
// in manual clock mode. Zero impact on sleep current — it only runs while the
// user woke the device anyway. main.cpp also retries it periodically while the
// clock is still approximate (e.g. WiFi unreachable on wake).
namespace OpportunisticTimeSync {

// Call once at boot, after clock restore + TZ setup and once the device has
// committed to staying awake. Spawns the background task only when a sync is
// warranted (approximate clock, stale sync, saved network); no-op otherwise.
void maybeStart();

// Foreground code that is about to take over the WiFi radio (activities doing
// their own connect) calls this so the background task aborts without touching
// WiFi state. Safe to call at any time, including when no task is running.
void claimForeground();

// Abort an in-flight sync AND release the radio (WiFi off, heap freed). Called
// when a book is opened so an ongoing sync never competes with the reader for
// heap/CPU on the single-core C3. No-op when no task is running.
void cancel();

}  // namespace OpportunisticTimeSync
