#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include <ctime>
#include <cstdlib>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Day/month/field names use I18n — build arrays at call site via tr()
static constexpr int FIELD_COUNT = 5;

bool isTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;
  return timeinfo.tm_year >= 125;  // Year >= 2025
}

int daysInMonth(int month, int year) {
  // month 0-based (tm_mon), year = tm_year (years since 1900)
  if (month == 1) {
    int y = year + 1900;
    return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
  }
  static const int days[] = {31,0,31,30,31,30,31,31,30,31,30,31};
  return days[month];
}
}  // namespace

void ClockActivity::applyEditedTime() {
  // Clamp day to valid range for the month
  int maxDay = daysInMonth(editTime.tm_mon, editTime.tm_year);
  if (editTime.tm_mday > maxDay) editTime.tm_mday = maxDay;

  // Let mktime derive DST — a zero-initialized editTime has tm_isdst=0 ("no DST"),
  // which shifts the saved time by 1h whenever the timezone is currently in DST (CEST)
  editTime.tm_isdst = -1;

  struct timeval tv;
  tv.tv_sec = mktime(&editTime);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

void ClockActivity::onEnter() {
  Activity::onEnter();
  editField = 0;
  monthOffset = 0;
  timeAvailable = isTimeValid();
  lastUpdateMs = millis();
  // Seed with the minute shown by the initial render so the first poll doesn't redraw
  struct tm timeinfo;
  lastRenderedMinute = (timeAvailable && getLocalTime(&timeinfo, 0)) ? timeinfo.tm_min : -1;

  // BUG-004: opportunistic NTP resync when user opens the clock screen while
  // WiFi is already up. Non-blocking — fires a background sync that updates
  // system time via sntp_set_time_sync_notification_cb (registered in main.cpp).
  // Users who periodically use WiFi (Sync, Weather, OTA) get their RTC drift
  // corrected without a dedicated "sync time" button.
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_NTP && WiFi.status() == WL_CONNECTED &&
      !esp_sntp_enabled()) {
    const char* tz = getenv("TZ");
    configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
  }

  // Manual mode: auto-enter time editing
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_MANUAL) {
    if (timeAvailable) {
      getLocalTime(&editTime, 0);
    } else {
      editTime = {};
      editTime.tm_year = 125;  // 2025
      editTime.tm_mon  = 0;
      editTime.tm_mday = 1;
    }
    editing = true;
  } else {
    editing = false;
  }

  requestUpdate();
}

void ClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (editing) {
      editing = false;
      timeAvailable = isTimeValid();
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!editing) {
      // Enter edit mode — seed with current time or default
      if (timeAvailable) {
        getLocalTime(&editTime, 0);
      } else {
        editTime = {};
        editTime.tm_year = 125;  // 2025
        editTime.tm_mon  = 0;
        editTime.tm_mday = 1;
      }
      editField = 0;
      editing = true;
    } else {
      // Save and exit edit mode
      applyEditedTime();
      editing = false;
      timeAvailable = true;
      lastUpdateMs = millis();
      lastRenderedMinute = editTime.tm_min;
    }
    requestUpdate();
    return;
  }

  if (editing) {
    bool changed = false;

    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      editField = (editField - 1 + FIELD_COUNT) % FIELD_COUNT;
      changed = true;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      editField = (editField + 1) % FIELD_COUNT;
      changed = true;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      switch (editField) {
        case 0: editTime.tm_hour = (editTime.tm_hour + 1) % 24; break;
        case 1: editTime.tm_min  = (editTime.tm_min  + 1) % 60; break;
        case 2: { int max = daysInMonth(editTime.tm_mon, editTime.tm_year);
                  editTime.tm_mday = editTime.tm_mday % max + 1; break; }
        case 3: editTime.tm_mon  = (editTime.tm_mon  + 1) % 12; break;
        case 4: editTime.tm_year = (editTime.tm_year < 200) ? editTime.tm_year + 1 : 125; break;
      }
      changed = true;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      switch (editField) {
        case 0: editTime.tm_hour = (editTime.tm_hour + 23) % 24; break;
        case 1: editTime.tm_min  = (editTime.tm_min  + 59) % 60; break;
        case 2: { int max = daysInMonth(editTime.tm_mon, editTime.tm_year);
                  editTime.tm_mday = (editTime.tm_mday - 2 + max) % max + 1; break; }
        case 3: editTime.tm_mon  = (editTime.tm_mon  + 11) % 12; break;
        case 4: editTime.tm_year = (editTime.tm_year > 125) ? editTime.tm_year - 1 : 125; break;
      }
      changed = true;
    }

    if (changed) requestUpdate();
    return;
  }

  // Month navigation with Left/Right in normal mode
  if (timeAvailable) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      monthOffset--;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      monthOffset++;
      requestUpdate();
      return;
    }
  }

  // Normal clock mode: re-check time validity; trigger immediate update on transition
  if (!timeAvailable) {
    if (isTimeValid()) {
      timeAvailable = true;
      lastRenderedMinute = -1;
      requestUpdate();
    }
  }

  // Redraw only when the displayed minute actually changes (poll at most once/second).
  // The old 10s blanket refresh re-rendered the full calendar + e-ink partial refresh
  // 6x/min with no visible change — wasted power and extra ghosting.
  if (timeAvailable && millis() - lastUpdateMs >= 1000) {
    lastUpdateMs = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0) && timeinfo.tm_min != lastRenderedMinute) {
      lastRenderedMinute = timeinfo.tm_min;
      requestUpdate();
    }
  }
}

void ClockActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CLOCK));

  if (editing) {
    // --- Edit mode ---
    const int timeH = renderer.getLineHeight(UI_12_FONT_ID);
    const int dateH = renderer.getLineHeight(UI_10_FONT_ID);
    const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
    const int totalH = timeH + 12 + dateH + 12 + labelH;
    const int startY = (pageHeight - totalH) / 2;

    // Time row: HH:MM with active field highlighted
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", editTime.tm_hour, editTime.tm_min);
    renderer.drawCenteredText(UI_12_FONT_ID, startY, timeBuf, true, EpdFontFamily::BOLD);

    // Date row
    char dateBuf[64];
    const char* monthNames[] = {tr(STR_MONTH_JAN), tr(STR_MONTH_FEB), tr(STR_MONTH_MAR),
        tr(STR_MONTH_APR), tr(STR_MONTH_MAY), tr(STR_MONTH_JUN), tr(STR_MONTH_JUL),
        tr(STR_MONTH_AUG), tr(STR_MONTH_SEP), tr(STR_MONTH_OCT), tr(STR_MONTH_NOV), tr(STR_MONTH_DEC)};
    snprintf(dateBuf, sizeof(dateBuf), "%d %s %d",
             editTime.tm_mday, monthNames[editTime.tm_mon], editTime.tm_year + 1900);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + timeH + 12, dateBuf);

    // Active field label
    const char* fieldLabels[] = {tr(STR_FIELD_HOUR), tr(STR_FIELD_MINUTE),
        tr(STR_FIELD_DAY), tr(STR_FIELD_MONTH), tr(STR_FIELD_YEAR)};
    char fieldBuf[32];
    snprintf(fieldBuf, sizeof(fieldBuf), "< %s >", fieldLabels[editField]);
    renderer.drawCenteredText(SMALL_FONT_ID, startY + timeH + 12 + dateH + 12, fieldBuf);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else if (!timeAvailable) {
    const int y = (pageHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_CONNECT_WIFI_TIME));

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CLOCK_SET_TIME), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else {
    struct tm timeinfo;
    getLocalTime(&timeinfo, 0);

    extern bool g_clockApproximate;
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%s%02d:%02d", g_clockApproximate ? "~" : "",
             timeinfo.tm_hour, timeinfo.tm_min);

    const char* dayNames[] = {tr(STR_DAY_SUN), tr(STR_DAY_MON), tr(STR_DAY_TUE), tr(STR_DAY_WED),
        tr(STR_DAY_THU), tr(STR_DAY_FRI), tr(STR_DAY_SAT)};
    const char* monthNames[] = {tr(STR_MONTH_JAN), tr(STR_MONTH_FEB), tr(STR_MONTH_MAR),
        tr(STR_MONTH_APR), tr(STR_MONTH_MAY), tr(STR_MONTH_JUN), tr(STR_MONTH_JUL),
        tr(STR_MONTH_AUG), tr(STR_MONTH_SEP), tr(STR_MONTH_OCT), tr(STR_MONTH_NOV), tr(STR_MONTH_DEC)};

    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d",
             dayNames[timeinfo.tm_wday], timeinfo.tm_mday,
             monthNames[timeinfo.tm_mon], timeinfo.tm_year + 1900);

    const int timeHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int dateHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int gregH  = renderer.getLineHeight(SMALL_FONT_ID);
    const int cellH  = gregH + 10;      // one row per calendar cell
    const int calH   = cellH * 7;       // header row + up to 6 body rows

    // Center the whole block in content area
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentBot = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int blockH = timeHeight + 8 + dateHeight + 12 + calH;
    const int startY = contentTop + (contentBot - contentTop - blockH) / 2;

    renderer.drawCenteredText(UI_12_FONT_ID, startY, timeBuf, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + timeHeight + 8, dateBuf);

    // Compute viewed month (apply monthOffset)
    struct tm viewTm = timeinfo;
    bool isCurrentMonth = (monthOffset == 0);
    if (!isCurrentMonth) {
      viewTm.tm_mon += monthOffset;
      viewTm.tm_mday = 1;
      mktime(&viewTm);  // normalizes month/year overflow
    }
    renderCalendar(startY + timeHeight + 8 + dateHeight + 12, viewTm, isCurrentMonth);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CLOCK_SET_TIME), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

void ClockActivity::renderCalendar(int startY, const struct tm& t, bool isCurrentMonth) const {
  const char* const DAY_HDR[] = {tr(STR_CAL_SUN), tr(STR_CAL_MON), tr(STR_CAL_TUE),
      tr(STR_CAL_WED), tr(STR_CAL_THU), tr(STR_CAL_FRI), tr(STR_CAL_SAT)};
  const char* monthNames[] = {tr(STR_MONTH_JAN), tr(STR_MONTH_FEB), tr(STR_MONTH_MAR),
      tr(STR_MONTH_APR), tr(STR_MONTH_MAY), tr(STR_MONTH_JUN), tr(STR_MONTH_JUL),
      tr(STR_MONTH_AUG), tr(STR_MONTH_SEP), tr(STR_MONTH_OCT), tr(STR_MONTH_NOV), tr(STR_MONTH_DEC)};
  const int pageWidth = renderer.getScreenWidth();
  const int margin = 20;
  const int calW = pageWidth - margin * 2;
  const int cellW = calW / 7;
  const int gregH = renderer.getLineHeight(SMALL_FONT_ID);
  const int cellH = gregH + 10;
  const int x0 = margin;

  // Month/year label
  int maxDay = daysInMonth(t.tm_mon, t.tm_year);

  char monthLabel[80];
  snprintf(monthLabel, sizeof(monthLabel), "< %s %d >",
           monthNames[t.tm_mon], t.tm_year + 1900);
  renderer.drawCenteredText(SMALL_FONT_ID, startY, monthLabel);

  int hdrY = startY + gregH + 6;

  // Day-of-week header row
  for (int d = 0; d < 7; d++) {
    int tw = renderer.getTextWidth(SMALL_FONT_ID, DAY_HDR[d]);
    renderer.drawText(SMALL_FONT_ID, x0 + d * cellW + (cellW - tw) / 2, hdrY, DAY_HDR[d]);
  }

  // First weekday of this month
  struct tm fm = t;
  fm.tm_mday = 1; fm.tm_hour = 0; fm.tm_min = 0; fm.tm_sec = 0;
  mktime(&fm);

  int col = fm.tm_wday;
  int rowY = hdrY + cellH;

  for (int day = 1; day <= maxDay; day++) {
    char gregBuf[4];
    snprintf(gregBuf, sizeof(gregBuf), "%d", day);

    const int cx    = x0 + col * cellW;
    const int gregX = cx + (cellW - renderer.getTextWidth(SMALL_FONT_ID, gregBuf)) / 2;

    const bool isToday = isCurrentMonth && (day == t.tm_mday);

    if (isToday) {
      renderer.fillRoundedRect(cx + 1, rowY - 2, cellW - 2, cellH - 1, 6, Color::Black);
      renderer.drawText(SMALL_FONT_ID, gregX, rowY, gregBuf, false);
    } else {
      renderer.drawText(SMALL_FONT_ID, gregX, rowY, gregBuf);
    }

    if (++col == 7) { col = 0; rowY += cellH; }
  }
}
