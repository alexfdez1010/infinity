#include "ReadingGoalsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "ReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "gamification/Gamification.h"
#include "util/StringUtils.h"

// Achievement pages 1/3..3/3 each show up to 8 badges (indices 0..7, 8..15, 16..23).
// If AchievementId grows past 24, badges beyond index 23 would silently never render.
static_assert(static_cast<uint8_t>(AchievementId::COUNT) <= 24,
              "ReadingGoalsActivity needs another achievements page beyond 24 badges");

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ReadingGoalsActivity::onEnter() {
  Activity::onEnter();
  page = 0;
  requestUpdate();
}

void ReadingGoalsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    page = (page + 1) % PAGE_COUNT;
    requestUpdate();
    return;
  }
  if (page == 0) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      GAMIFY.adjustGoal(-5);
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      GAMIFY.adjustGoal(+5);
      requestUpdate();
    }
  }
}

// ── Render ────────────────────────────────────────────────────────────────────

namespace {
constexpr int MARGIN = 28;
}

void ReadingGoalsActivity::render(RenderLock&&) {
  const int pageWidth = renderer.getScreenWidth();
  renderer.clearScreen();

  const int lhUi12 = renderer.getLineHeight(UI_12_FONT_ID);
  const int sepMargin = MARGIN + 20;
  const int sepW = pageWidth - 2 * sepMargin;

  int y = 24;
  renderer.drawCenteredText(UI_12_FONT_ID, y,
                            page == 0 ? tr(STR_READING_GOALS) : tr(STR_ACHIEVEMENTS), true,
                            EpdFontFamily::BOLD);
  y += lhUi12 + 6;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  if (page == 0) {
    renderGoalsPage(y);
  } else {
    renderAchievementsPage(y, static_cast<uint8_t>((page - 1) * 8));
  }

  // Button hints
  if (page == 0) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GOALS_SWITCH_PAGE),
                                              tr(STR_GOAL_MINUS), tr(STR_GOAL_PLUS));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GOALS_SWITCH_PAGE), nullptr, nullptr);
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, nullptr, nullptr);
  }

  renderer.displayBuffer();
}

int ReadingGoalsActivity::renderGoalsPage(int y) {
  const int pageWidth = renderer.getScreenWidth();
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhUi10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhUi12 = renderer.getLineHeight(UI_12_FONT_ID);
  const int sepMargin = MARGIN + 20;
  const int sepW = pageWidth - 2 * sepMargin;

  // --- STREAK card: current | freezes | best ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STREAK_SECTION));
  y += lhSmall + 4;
  {
    const int colW = (pageWidth - MARGIN * 2) / 3;
    const char* labels[3] = {tr(STR_STATS_STREAK), tr(STR_STREAK_FREEZES), tr(STR_STATS_BEST_STREAK)};
    char values[3][16];
    snprintf(values[0], sizeof(values[0]), tr(STR_STATS_DAYS), (unsigned)GAMIFY.streak);
    snprintf(values[1], sizeof(values[1]), "%u", (unsigned)GAMIFY.freezeTokens);
    snprintf(values[2], sizeof(values[2]), tr(STR_STATS_DAYS), (unsigned)GAMIFY.longestStreak);

    const int cardPad = 8;
    const int cardH = lhUi12 + lhSmall + 8 + cardPad * 2;
    renderer.drawRoundedRect(MARGIN - cardPad, y - cardPad, pageWidth - MARGIN * 2 + cardPad * 2,
                             cardH, 1, 10, true);
    for (int i = 1; i < 3; i++) {
      const int divX = MARGIN + i * colW;
      renderer.drawLine(divX, y - cardPad + 4, divX, y + lhUi12 + lhSmall + 8 + cardPad - 4);
    }
    for (int i = 0; i < 3; i++) {
      const int cx = MARGIN + i * colW + colW / 2;
      const int vw = renderer.getTextWidth(UI_12_FONT_ID, values[i]);
      renderer.drawText(UI_12_FONT_ID, cx - vw / 2, y, values[i], true, EpdFontFamily::BOLD);
      const int lw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
      renderer.drawText(SMALL_FONT_ID, cx - lw / 2, y + lhUi12 + 2, labels[i]);
    }
    y += lhUi12 + lhSmall + 8 + cardPad + 14;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- DAILY GOAL: target + today's progress bar ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_GOAL_DAILY));
  y += lhSmall + 4;

  const uint32_t goalSecs = GAMIFY.dailyGoalMinutes * 60UL;
  const uint32_t todaySecs = GAMIFY.liveTodaySeconds();
  char goalBuf[48];
  snprintf(goalBuf, sizeof(goalBuf), tr(STR_GOAL_TARGET_FMT), (unsigned)GAMIFY.dailyGoalMinutes);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, goalBuf, true, EpdFontFamily::BOLD);
  if (GAMIFY.goalMetToday()) {
    const int gw = renderer.getTextWidth(UI_10_FONT_ID, goalBuf, EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, MARGIN + gw + 12, y + (lhUi10 - lhSmall) / 2, tr(STR_GOAL_MET));
  }
  y += lhUi10 + 8;

  // Progress bar (today vs goal)
  {
    constexpr int BAR_H = 12;
    const int barW = pageWidth - MARGIN * 2;
    renderer.drawRoundedRect(MARGIN, y, barW, BAR_H, 1, 4, true);
    uint32_t pct = goalSecs > 0 ? (todaySecs * 100) / goalSecs : 0;
    if (pct > 100) pct = 100;
    const int filledW = static_cast<int>((barW - 4) * pct / 100);
    if (filledW > 0) renderer.fillRect(MARGIN + 2, y + 2, filledW, BAR_H - 4);
    y += BAR_H + 4;

    char progBuf[48];
    snprintf(progBuf, sizeof(progBuf), tr(STR_GOAL_PROGRESS_FMT), (unsigned)(todaySecs / 60),
             (unsigned)GAMIFY.dailyGoalMinutes);
    renderer.drawText(SMALL_FONT_ID, MARGIN, y, progBuf);
    y += lhSmall + 12;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- DAILY QUEST: rotating micro-challenge ---
  const QuestId quest = GAMIFY.todaysQuest();
  if (quest != QuestId::COUNT) {
    renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_QUEST_SECTION));
    if (GAMIFY.questsCompletedTotal > 0) {
      char totalBuf[32];
      snprintf(totalBuf, sizeof(totalBuf), tr(STR_QUEST_TOTAL_FMT),
               (unsigned)GAMIFY.questsCompletedTotal);
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, totalBuf);
      renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - tw, y, totalBuf);
    }
    y += lhSmall + 4;

    // Done-state indicator square + quest text
    constexpr int BADGE = 14;
    const int badgeY = y + (lhUi10 - BADGE) / 2;
    if (GAMIFY.questDoneToday) {
      renderer.fillRect(MARGIN, badgeY, BADGE, BADGE);
    } else {
      renderer.drawRect(MARGIN, badgeY, BADGE, BADGE);
    }
    const int questX = MARGIN + BADGE + 10;
    renderer.drawText(UI_10_FONT_ID, questX, y, I18N.get(GamificationManager::questDesc(quest)),
                      true, GAMIFY.questDoneToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    if (GAMIFY.questDoneToday) {
      const int qw = renderer.getTextWidth(UI_10_FONT_ID,
                                           I18N.get(GamificationManager::questDesc(quest)),
                                           EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, questX + qw + 10, y + (lhUi10 - lhSmall) / 2,
                        tr(STR_QUEST_DONE));
    }
    y += lhUi10 + 12;
    renderer.fillRect(sepMargin, y, sepW, 1);
    y += 14;
  }

  // --- LAST 7 DAYS bar chart ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_GOAL_LAST_7_DAYS));
  {
    // Weekly goal-days tally, right-aligned on the section label line
    char weekBuf[32];
    snprintf(weekBuf, sizeof(weekBuf), tr(STR_WEEK_GOALS_FMT), (unsigned)GAMIFY.weekGoalDays());
    const int ww = renderer.getTextWidth(SMALL_FONT_ID, weekBuf);
    renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - ww, y, weekBuf);
  }
  y += lhSmall + 6;
  {
    constexpr int CHART_H = 90;
    const int chartW = pageWidth - MARGIN * 2;
    const int slotW = chartW / 7;
    const int barW = slotW - 10;

    // Scale: max of goal and best day in ring, so the goal line stays on-chart
    uint16_t maxVal = GAMIFY.dailyGoalMinutes;
    for (int i = 0; i < 7; i++) {
      if (GAMIFY.weekMinutes[i] > maxVal) maxVal = GAMIFY.weekMinutes[i];
    }
    if (maxVal == 0) maxVal = 1;

    const int baseY = y + CHART_H;
    // Goal threshold: dashed horizontal line
    const int goalY = baseY - static_cast<int>(CHART_H * GAMIFY.dailyGoalMinutes / maxVal);
    for (int x = MARGIN; x < MARGIN + chartW; x += 8) {
      renderer.fillRect(x, goalY, 4, 1);
    }
    // Bars: [6]=oldest on the left … [0]=today on the right
    for (int i = 0; i < 7; i++) {
      const uint16_t v = GAMIFY.weekMinutes[6 - i];
      const int h = static_cast<int>(CHART_H * v / maxVal);
      const int bx = MARGIN + i * slotW + (slotW - barW) / 2;
      if (h > 0) {
        renderer.fillRect(bx, baseY - h, barW, h);
      } else {
        renderer.fillRect(bx, baseY - 1, barW, 1);  // empty-day tick
      }
    }
    renderer.fillRect(MARGIN, baseY, chartW, 1);  // baseline
    y = baseY + 6;
    // Today marker under the rightmost bar
    const char* todayLbl = tr(STR_GOAL_TODAY);
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, todayLbl);
    renderer.drawText(SMALL_FONT_ID, MARGIN + 6 * slotW + (slotW - tw) / 2, y, todayLbl);
    y += lhSmall + 12;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- RECORDS ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_GOAL_RECORDS));
  y += lhSmall + 4;
  {
    char buf[32];
    StringUtils::formatReadingDuration(buf, sizeof(buf), GAMIFY.bestSessionSeconds);
    renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_GOAL_BEST_SESSION));
    const int vw = renderer.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, pageWidth - MARGIN - vw, y - (lhUi10 - lhSmall) / 2, buf, true,
                      EpdFontFamily::BOLD);
    y += lhUi10 + 6;

    StringUtils::formatReadingDuration(buf, sizeof(buf), GAMIFY.bestDaySeconds);
    renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_GOAL_BEST_DAY));
    const int vw2 = renderer.getTextWidth(UI_10_FONT_ID, buf, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, pageWidth - MARGIN - vw2, y - (lhUi10 - lhSmall) / 2, buf, true,
                      EpdFontFamily::BOLD);
    y += lhUi10 + 6;
  }
  return y;
}

int ReadingGoalsActivity::renderAchievementsPage(int y, uint8_t firstIdx) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);

  // "N / 15 unlocked" counter
  char countBuf[32];
  snprintf(countBuf, sizeof(countBuf), tr(STR_ACH_COUNT_FMT), (unsigned)GAMIFY.unlockedCount(),
           (unsigned)AchievementId::COUNT);
  renderer.drawCenteredText(SMALL_FONT_ID, y, countBuf);
  y += lhSmall + 12;

  constexpr int BADGE = 14;  // unlocked/locked indicator square
  const int rowH = lhSmall * 2 + 12;
  const int maxY = pageHeight - 50;
  const uint8_t lastIdx = firstIdx + 8 < static_cast<uint8_t>(AchievementId::COUNT)
                              ? firstIdx + 8
                              : static_cast<uint8_t>(AchievementId::COUNT);

  for (uint8_t i = firstIdx; i < lastIdx && y + rowH <= maxY; i++) {
    const auto& def = achievementDef(i);
    const bool unlocked = GAMIFY.isUnlocked(static_cast<AchievementId>(i));

    const int badgeY = y + (lhSmall - BADGE) / 2 + 2;
    if (unlocked) {
      renderer.fillRect(MARGIN, badgeY, BADGE, BADGE);
    } else {
      renderer.drawRect(MARGIN, badgeY, BADGE, BADGE);
    }

    const int textX = MARGIN + BADGE + 10;
    renderer.drawText(SMALL_FONT_ID, textX, y, I18N.get(def.name), true,
                      unlocked ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, textX, y + lhSmall + 2, I18N.get(def.desc));
    y += rowH;

    if (i + 1 < lastIdx) {
      renderer.fillRect(MARGIN, y - 5, pageWidth - MARGIN * 2, 1);
    }
  }
  return y;
}
