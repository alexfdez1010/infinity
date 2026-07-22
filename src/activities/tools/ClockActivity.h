#pragma once
#include "../Activity.h"

class ClockActivity final : public Activity {
  unsigned long lastUpdateMs = 0;
  int lastRenderedMinute = -1;  // -1 forces a redraw on next time poll
  bool timeAvailable = false;

  // Manual time-set mode
  bool editing = false;
  int editField = 0;  // 0=hour, 1=minute, 2=day, 3=month, 4=year
  struct tm editTime = {};

  // Month navigation (0 = current month, +1 = next, -1 = prev)
  int monthOffset = 0;

  // Long-press Down = +1 day (offline date fix without an NTP sync)
  bool addDayTriggered = false;
  unsigned long dayAddedMsgExpiry = 0;  // 0 = no message shown

  void applyEditedTime();
  void addOneDay();
  void renderCalendar(int startY, const struct tm& t, bool isCurrentMonth) const;

 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
