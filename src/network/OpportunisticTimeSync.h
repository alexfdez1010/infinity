#pragma once

#include <cstdint>

// Opportunistic background NTP resync on wake.
//
// The ESP32-C3 has no battery-backed RTC: after deep sleep the clock is restored
// from the drift-prone internal RC timer (g_clockApproximate). When the last NTP
// sync is stale, this module blindly connects to a saved WiFi network (last-used
// first), syncs time via SNTP, and switches the radio back off. Skipped entirely
// in manual clock mode. main.cpp retries it periodically while the clock is still
// approximate (e.g. WiFi unreachable on wake).
//
// It runs as a state machine driven from the main loop (poll()), NOT a background
// task: WiFi.mode(WIFI_STA) must be called from the main Arduino task — from a
// separate low-priority task at tight heap it hard-froze the whole device. poll()
// advances only one small step per iteration, so the UI never blocks.
namespace OpportunisticTimeSync {

// Default heap floor below which a sync is not even attempted (the radio needs a
// big contiguous block to come up).
constexpr uint32_t DEFAULT_MIN_FREE_HEAP = 55000;

// Arm a sync when one is warranted (approximate clock, saved network, enough free
// heap, not in manual mode, WiFi not already up). No-op otherwise. The actual
// work happens in poll(). Call from the main loop. minFreeHeap raises the heap
// floor for callers with less headroom to spare (e.g. while a book is open).
void maybeStart(uint32_t minFreeHeap = DEFAULT_MIN_FREE_HEAP);

// Advance the sync state machine one step. Call every main-loop iteration. Cheap
// (a WiFi.status() check) except the one-time WiFi.mode(WIFI_STA) start.
void poll();

// True while a sync is armed or in progress. Activities that hold large heap
// caches (Home/Recents 48KB frame buffers) consult this to stop re-caching so the
// WiFi radio can get the contiguous heap it needs to come up.
bool busy();

// Foreground code that is about to take over the WiFi radio (activities doing
// their own connect) calls this so the background task aborts without touching
// WiFi state. Safe to call at any time, including when no task is running.
void claimForeground();

// Abort an in-flight sync AND release the radio (WiFi off, heap freed). Called
// when a book is opened so an ongoing sync never competes with the reader for
// heap/CPU on the single-core C3. No-op when no task is running.
void cancel();

}  // namespace OpportunisticTimeSync
