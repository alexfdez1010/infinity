#pragma once
#include "../Activity.h"

// Reading gamification dashboard: daily goal + streak + weekly chart on page 0,
// achievement badge list on page 1. Confirm toggles pages, Left/Right adjust
// the daily goal, Back exits.
class ReadingGoalsActivity final : public Activity {
 public:
  explicit ReadingGoalsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingGoals", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int PAGE_COUNT = 4;  // goals, achievements 1/3, 2/3, 3/3
  int page = 0;

  int renderGoalsPage(int y);
  int renderAchievementsPage(int y, uint8_t firstIdx);
};
