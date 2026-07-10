#include "Gamification.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "ReadingStats.h"

namespace {
constexpr time_t MIN_VALID_TIME = 1704067200;  // 2024-01-01 00:00:00 UTC (NTP synced)
constexpr uint16_t GOAL_MIN = 5;
constexpr uint16_t GOAL_MAX = 240;
constexpr uint8_t MAX_FREEZE_TOKENS = 3;
constexpr uint8_t STREAK_DAYS_PER_TOKEN = 7;
constexpr uint8_t MAX_BRIDGEABLE_GAP = 3;  // gap of 2-3 days needs 1-2 tokens; more breaks the streak

int daysInYear(int tmYear) {
  const int y = tmYear + 1900;
  return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;
}

const AchievementDef ACHIEVEMENTS[static_cast<uint8_t>(AchievementId::COUNT)] = {
    {StrId::STR_ACH_FIRST_BOOK, StrId::STR_ACH_FIRST_BOOK_DESC},
    {StrId::STR_ACH_BOOKS_5, StrId::STR_ACH_BOOKS_5_DESC},
    {StrId::STR_ACH_BOOKS_25, StrId::STR_ACH_BOOKS_25_DESC},
    {StrId::STR_ACH_STREAK_7, StrId::STR_ACH_STREAK_7_DESC},
    {StrId::STR_ACH_STREAK_30, StrId::STR_ACH_STREAK_30_DESC},
    {StrId::STR_ACH_STREAK_100, StrId::STR_ACH_STREAK_100_DESC},
    {StrId::STR_ACH_HOURS_10, StrId::STR_ACH_HOURS_10_DESC},
    {StrId::STR_ACH_HOURS_50, StrId::STR_ACH_HOURS_50_DESC},
    {StrId::STR_ACH_HOURS_100, StrId::STR_ACH_HOURS_100_DESC},
    {StrId::STR_ACH_HOURS_500, StrId::STR_ACH_HOURS_500_DESC},
    {StrId::STR_ACH_DEEP_SESSION, StrId::STR_ACH_DEEP_SESSION_DESC},
    {StrId::STR_ACH_MARATHON_DAY, StrId::STR_ACH_MARATHON_DAY_DESC},
    {StrId::STR_ACH_GOAL_WEEK, StrId::STR_ACH_GOAL_WEEK_DESC},
    {StrId::STR_ACH_GOAL_30, StrId::STR_ACH_GOAL_30_DESC},
    {StrId::STR_ACH_SESSIONS_100, StrId::STR_ACH_SESSIONS_100_DESC},
    {StrId::STR_ACH_BOOKS_50, StrId::STR_ACH_BOOKS_50_DESC},
    {StrId::STR_ACH_STREAK_365, StrId::STR_ACH_STREAK_365_DESC},
    {StrId::STR_ACH_GOAL_100, StrId::STR_ACH_GOAL_100_DESC},
    {StrId::STR_ACH_EARLY_BIRD, StrId::STR_ACH_EARLY_BIRD_DESC},
    {StrId::STR_ACH_NIGHT_OWL, StrId::STR_ACH_NIGHT_OWL_DESC},
    {StrId::STR_ACH_QUESTS_25, StrId::STR_ACH_QUESTS_25_DESC},
};

// Current local hour (0-23), or -1 when the wall clock is not valid yet
int currentHour() {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return -1;
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  return timeinfo.tm_hour;
}
}  // namespace

const AchievementDef& achievementDef(uint8_t idx) { return ACHIEVEMENTS[idx]; }

GamificationManager GamificationManager::instance;
constexpr char GamificationManager::STATE_PATH[];

GamificationManager& GamificationManager::getInstance() { return instance; }

uint8_t GamificationManager::unlockedCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < static_cast<uint8_t>(AchievementId::COUNT); i++) {
    if (unlockedMask & (1UL << i)) count++;
  }
  return count;
}

uint32_t GamificationManager::liveTodaySeconds() const {
  // ReadingStats only rolls its today-counter at endSession, so during the first
  // session of a new day it still holds yesterday's seconds — ignore it then.
  uint32_t base = READ_STATS.todayReadSeconds;
  const time_t now = time(nullptr);
  if (now >= MIN_VALID_TIME) {
    struct tm timeinfo = {};
    localtime_r(&now, &timeinfo);
    if (static_cast<int16_t>(timeinfo.tm_yday) != READ_STATS.lastReadDayOfYear ||
        static_cast<int16_t>(timeinfo.tm_year) != READ_STATS.lastReadYear) {
      base = 0;
    }
  }
  return base + READ_STATS.currentSessionSeconds();
}

void GamificationManager::adjustGoal(int deltaMinutes) {
  int goal = static_cast<int>(dailyGoalMinutes) + deltaMinutes;
  if (goal < GOAL_MIN) goal = GOAL_MIN;
  if (goal > GOAL_MAX) goal = GOAL_MAX;
  if (goal != dailyGoalMinutes) {
    dailyGoalMinutes = static_cast<uint16_t>(goal);
    saveToFile();
  }
}

void GamificationManager::rollDay() {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return;  // no valid wall clock, don't touch day state
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  const int16_t curYear = static_cast<int16_t>(timeinfo.tm_year);
  const int16_t curDay = static_cast<int16_t>(timeinfo.tm_yday);

  if (lastDayOfYear < 0) {
    // First reading day ever
    lastYear = curYear;
    lastDayOfYear = curDay;
    if (streak == 0) streak = 1;
    if (streak > longestStreak) longestStreak = streak;
    return;
  }
  if (curYear == lastYear && curDay == lastDayOfYear) return;  // same day

  // Days elapsed since last recorded reading day
  int gap;
  if (curYear == lastYear) {
    gap = curDay - lastDayOfYear;
  } else if (curYear == lastYear + 1) {
    gap = daysInYear(lastYear) - lastDayOfYear + curDay;
  } else if (curYear > lastYear) {
    gap = 999;  // more than a year elapsed; treat as a broken streak
  } else {
    return;  // clock moved backwards across a year boundary; ignore
  }
  if (gap <= 0) return;  // clock moved backwards within the same/adjacent year; ignore

  // Close the previous day: goal streak bookkeeping from the ring's today slot
  const bool metLastDay = weekMinutes[0] >= dailyGoalMinutes;
  if (metLastDay) goalDaysTotal++;
  goalStreak = (gap == 1 && metLastDay) ? static_cast<uint16_t>(goalStreak + 1) : 0;

  // Shift week ring by gap days
  const int shift = gap > 7 ? 7 : gap;
  for (int i = 6; i >= 0; i--) {
    weekMinutes[i] = (i - shift >= 0) ? weekMinutes[i - shift] : 0;
  }

  // Streak: consecutive day extends it; short gaps consume freeze tokens
  if (gap == 1) {
    streak++;
  } else if (gap <= MAX_BRIDGEABLE_GAP && freezeTokens >= gap - 1) {
    freezeTokens -= static_cast<uint8_t>(gap - 1);
    streak++;
  } else {
    streak = 1;
  }
  if (streak > longestStreak) longestStreak = streak;
  if (streak % STREAK_DAYS_PER_TOKEN == 0 && freezeTokens < MAX_FREEZE_TOKENS) {
    freezeTokens++;
  }

  goalCelebrated = false;
  nudgeGiven = false;
  questDoneToday = false;
  todaySessions = 0;
  chaptersToday = 0;
  lastYear = curYear;
  lastDayOfYear = curDay;
}

QuestId GamificationManager::todaysQuest() const {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return QuestId::COUNT;
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  // Deterministic per calendar day, avoids repeating yesterday's quest on most days
  const uint32_t seed = static_cast<uint32_t>(timeinfo.tm_year) * 366u +
                        static_cast<uint32_t>(timeinfo.tm_yday);
  return static_cast<QuestId>((seed * 2654435761u >> 8) % static_cast<uint8_t>(QuestId::COUNT));
}

StrId GamificationManager::questDesc(QuestId id) {
  switch (id) {
    case QuestId::FocusSession: return StrId::STR_QUEST_FOCUS;
    case QuestId::Overachieve:  return StrId::STR_QUEST_OVERACHIEVE;
    case QuestId::TwoSessions:  return StrId::STR_QUEST_TWO_SESSIONS;
    case QuestId::Chapter:      return StrId::STR_QUEST_CHAPTER;
    default:                    return StrId::STR_QUEST_CHAPTERS_2;
  }
}

uint8_t GamificationManager::weekGoalDays() const {
  uint8_t days = 0;
  for (int i = 0; i < 7; i++) {
    if (weekMinutes[i] >= dailyGoalMinutes) days++;
  }
  // Ring slot [0] only updates at session end; count a live goal met today too
  if (weekMinutes[0] < dailyGoalMinutes && goalMetToday()) days++;
  return days;
}

bool GamificationManager::questConditionMet() const {
  switch (todaysQuest()) {
    case QuestId::FocusSession:
      return READ_STATS.currentSessionSeconds() >= 20UL * 60;
    case QuestId::Overachieve:
      return liveTodaySeconds() >= dailyGoalMinutes * 90UL;  // 60 * 1.5
    case QuestId::TwoSessions:
      return todaySessions >= 2;
    case QuestId::Chapter:
      return chaptersToday >= 1;
    case QuestId::Chapters2:
      return chaptersToday >= 2;
    default:
      return false;  // no valid clock, no quest
  }
}

void GamificationManager::rollSurpriseReward() {
  if (freezeTokens >= MAX_FREEZE_TOKENS) return;
  // Deterministic but unpredictable-feeling: hash of date + lifetime session count
  const uint32_t h = (static_cast<uint32_t>(lastYear) * 366u + static_cast<uint32_t>(lastDayOfYear)) *
                         2654435761u ^
                     READ_STATS.totalSessions * 40503u;
  if ((h >> 4) % 3 == 0) {
    freezeTokens++;
    surprisePending = true;
  }
}

AchievementId GamificationManager::evaluateAchievements() {
  const uint32_t totalSecs = READ_STATS.totalReadSeconds + READ_STATS.currentSessionSeconds();
  const uint32_t todaySecs = liveTodaySeconds();
  const uint32_t sessionSecs = READ_STATS.currentSessionSeconds();
  const uint16_t effStreak = streak > READ_STATS.currentStreak ? streak : READ_STATS.currentStreak;
  // Goal streak including today if already met
  const uint16_t liveGoalStreak = goalStreak + (goalCelebrated ? 1 : 0);
  const uint16_t liveGoalDays = goalDaysTotal + (goalCelebrated ? 1 : 0);
  const int hour = currentHour();

  const bool met[static_cast<uint8_t>(AchievementId::COUNT)] = {
      READ_STATS.booksFinished >= 1,
      READ_STATS.booksFinished >= 5,
      READ_STATS.booksFinished >= 25,
      effStreak >= 7,
      effStreak >= 30,
      effStreak >= 100,
      totalSecs >= 10UL * 3600,
      totalSecs >= 50UL * 3600,
      totalSecs >= 100UL * 3600,
      totalSecs >= 500UL * 3600,
      sessionSecs >= 3600 || bestSessionSeconds >= 3600,
      todaySecs >= 3UL * 3600 || bestDaySeconds >= 3UL * 3600,
      liveGoalStreak >= 7,
      liveGoalDays >= 30,
      READ_STATS.totalSessions >= 100,
      READ_STATS.booksFinished >= 50,
      effStreak >= 365,
      liveGoalDays >= 100,
      // Momentary conditions: pollReader runs on every page turn, so the
      // qualifying moment is observed while it is true
      hour >= 0 && hour < 9 && goalMetToday(),
      (hour >= 23 || (hour >= 0 && hour < 4)) && sessionSecs > 0,
      questsCompletedTotal >= 25,
  };

  AchievementId firstNew = AchievementId::COUNT;
  for (uint8_t i = 0; i < static_cast<uint8_t>(AchievementId::COUNT); i++) {
    const uint32_t bit = 1UL << i;
    if (met[i] && !(unlockedMask & bit)) {
      unlockedMask |= bit;
      if (firstNew == AchievementId::COUNT) firstNew = static_cast<AchievementId>(i);
      LOG_DBG("GAM", "Achievement unlocked: %u", i);
    }
  }
  return firstNew;
}

bool GamificationManager::pollReader(char* toast, size_t toastLen) {
  if (!toast || toastLen == 0) return false;

  // Queued surprise reward from an earlier goal/quest completion
  if (surprisePending) {
    surprisePending = false;
    saveToFile();
    snprintf(toast, toastLen, "%s", tr(STR_SURPRISE_TOAST));
    return true;
  }

  // Daily goal crossed mid-session: celebrate once per day
  if (!goalCelebrated && dailyGoalMinutes > 0 && goalMetToday()) {
    goalCelebrated = true;
    rollSurpriseReward();    // variable-ratio bonus, toasted on the next poll
    evaluateAchievements();  // goal-based badges may unlock at the same moment
    saveToFile();
    snprintf(toast, toastLen, "%s", tr(STR_GOAL_MET_TOAST));
    return true;
  }

  // Daily quest completed mid-session
  if (!questDoneToday && questConditionMet()) {
    questDoneToday = true;
    questsCompletedTotal++;
    rollSurpriseReward();
    evaluateAchievements();
    saveToFile();
    snprintf(toast, toastLen, "%s", tr(STR_QUEST_DONE_TOAST));
    return true;
  }

  // Goal-gradient nudge: within the last 5 minutes of the goal (and past halfway)
  if (!goalCelebrated && !nudgeGiven && dailyGoalMinutes > 0) {
    const uint32_t goalSecs = dailyGoalMinutes * 60UL;
    const uint32_t todaySecs = liveTodaySeconds();
    if (todaySecs * 2 >= goalSecs && todaySecs < goalSecs && goalSecs - todaySecs <= 5UL * 60) {
      nudgeGiven = true;
      const unsigned minsLeft = static_cast<unsigned>((goalSecs - todaySecs + 59) / 60);
      snprintf(toast, toastLen, tr(STR_GOAL_NUDGE_FMT), minsLeft);
      return true;
    }
  }

  const AchievementId newUnlock = evaluateAchievements();
  if (newUnlock != AchievementId::COUNT) {
    saveToFile();
    snprintf(toast, toastLen, "%s %s", tr(STR_ACH_UNLOCKED),
             I18N.get(achievementDef(static_cast<uint8_t>(newUnlock)).name));
    return true;
  }
  return false;
}

void GamificationManager::onSessionEnd() {
  rollDay();

  // Today's minutes from the authoritative counter (session already accumulated)
  weekMinutes[0] = static_cast<uint16_t>(READ_STATS.todayReadSeconds / 60);

  // Personal records via total-seconds delta (survives missed polls)
  uint32_t sessionSecs = 0;
  if (READ_STATS.totalReadSeconds >= lastKnownTotalSeconds) {
    sessionSecs = READ_STATS.totalReadSeconds - lastKnownTotalSeconds;
    if (sessionSecs > bestSessionSeconds && sessionSecs < 86400) bestSessionSeconds = sessionSecs;
  }
  lastKnownTotalSeconds = READ_STATS.totalReadSeconds;
  if (READ_STATS.todayReadSeconds > bestDaySeconds) bestDaySeconds = READ_STATS.todayReadSeconds;

  // Goal fallback (e.g. reader closed right after crossing, poll missed it)
  if (!goalCelebrated && dailyGoalMinutes > 0 &&
      READ_STATS.todayReadSeconds >= dailyGoalMinutes * 60UL) {
    goalCelebrated = true;
  }

  // Quest fallback: conditions that hold at session end but were missed by polls
  // (FocusSession checked against the closed session's length, since the live
  // session counter is already 0 here)
  if (!questDoneToday) {
    const bool focusMet = todaysQuest() == QuestId::FocusSession && sessionSecs >= 20UL * 60;
    if (focusMet || questConditionMet()) {
      questDoneToday = true;
      questsCompletedTotal++;
      rollSurpriseReward();
      surprisePending = false;  // no reader to toast it; token still granted
    }
  }

  evaluateAchievements();
  saveToFile();
}

bool GamificationManager::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  JsonDocument doc;
  doc["dailyGoalMinutes"] = dailyGoalMinutes;
  doc["streak"] = streak;
  doc["longestStreak"] = longestStreak;
  doc["freezeTokens"] = freezeTokens;
  doc["goalStreak"] = goalStreak;
  doc["goalDaysTotal"] = goalDaysTotal;
  doc["unlockedMask"] = unlockedMask;
  doc["bestSessionSeconds"] = bestSessionSeconds;
  doc["bestDaySeconds"] = bestDaySeconds;
  doc["lastYear"] = lastYear;
  doc["lastDayOfYear"] = lastDayOfYear;
  doc["goalCelebrated"] = goalCelebrated;
  doc["lastKnownTotalSeconds"] = lastKnownTotalSeconds;
  doc["questsCompletedTotal"] = questsCompletedTotal;
  doc["questDoneToday"] = questDoneToday;
  doc["todaySessions"] = todaySessions;
  doc["chaptersToday"] = chaptersToday;
  JsonArray week = doc["weekMinutes"].to<JsonArray>();
  for (int i = 0; i < 7; i++) week.add(weekMinutes[i]);

  String json;
  serializeJson(doc, json);
  const bool ok = Storage.writeFile(STATE_PATH, json);
  if (!ok) LOG_ERR("GAM", "Failed to save gamification state");
  return ok;
}

bool GamificationManager::loadFromFile() {
  if (!Storage.exists(STATE_PATH)) {
    // First run: don't count all reading history as one giant session record
    lastKnownTotalSeconds = READ_STATS.totalReadSeconds;
    // Seed streak from ReadingStats so long-time users don't start from zero
    streak = READ_STATS.currentStreak;
    longestStreak = READ_STATS.longestStreak;
    return true;
  }
  String json = Storage.readFile(STATE_PATH);
  if (json.isEmpty()) return false;
  JsonDocument doc;
  const auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("GAM", "Gamification state JSON parse error: %s", error.c_str());
    return false;
  }
  dailyGoalMinutes = doc["dailyGoalMinutes"] | (uint16_t)20;
  if (dailyGoalMinutes < GOAL_MIN) dailyGoalMinutes = GOAL_MIN;
  if (dailyGoalMinutes > GOAL_MAX) dailyGoalMinutes = GOAL_MAX;
  streak = doc["streak"] | (uint16_t)0;
  longestStreak = doc["longestStreak"] | (uint16_t)0;
  freezeTokens = doc["freezeTokens"] | (uint8_t)0;
  goalStreak = doc["goalStreak"] | (uint16_t)0;
  goalDaysTotal = doc["goalDaysTotal"] | (uint16_t)0;
  unlockedMask = doc["unlockedMask"] | (uint32_t)0;
  bestSessionSeconds = doc["bestSessionSeconds"] | (uint32_t)0;
  bestDaySeconds = doc["bestDaySeconds"] | (uint32_t)0;
  lastYear = doc["lastYear"] | (int16_t)0;
  lastDayOfYear = doc["lastDayOfYear"] | (int16_t)-1;
  goalCelebrated = doc["goalCelebrated"] | false;
  lastKnownTotalSeconds = doc["lastKnownTotalSeconds"] | READ_STATS.totalReadSeconds;
  questsCompletedTotal = doc["questsCompletedTotal"] | (uint16_t)0;
  questDoneToday = doc["questDoneToday"] | false;
  todaySessions = doc["todaySessions"] | (uint8_t)0;
  chaptersToday = doc["chaptersToday"] | (uint8_t)0;
  JsonArray week = doc["weekMinutes"];
  for (int i = 0; i < 7 && i < static_cast<int>(week.size()); i++) {
    weekMinutes[i] = week[i] | (uint16_t)0;
  }
  return true;
}
