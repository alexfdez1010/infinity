#pragma once

bool isClockApproximate();
void setClockApproximate(bool approximate);

// Signals that the system clock jumped (NTP sync or a manual change from the clock
// screen). The main loop picks this up and splits an active reading session across
// the day boundary, so moving the date forward never inflates today's reading time.
void notifyClockChanged();
