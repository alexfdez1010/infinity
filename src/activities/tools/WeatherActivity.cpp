#include "WeatherActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <cstdlib>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "components/icons/weather.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "network/OpportunisticTimeSync.h"

// 50 provincial capitals of Spain, sorted alphabetically
const CityCoord WeatherActivity::CITIES[CITY_COUNT] = {
    {"A Coruña", "43.3623", "-8.4115"},
    {"Albacete", "38.9943", "-1.8585"},
    {"Alicante", "38.3452", "-0.4810"},
    {"Almería", "36.8340", "-2.4637"},
    {"Ávila", "40.6566", "-4.6812"},
    {"Badajoz", "38.8794", "-6.9707"},
    {"Barcelona", "41.3874", "2.1686"},
    {"Bilbao", "43.2630", "-2.9350"},
    {"Burgos", "42.3439", "-3.6969"},
    {"Cáceres", "39.4753", "-6.3724"},
    {"Cádiz", "36.5271", "-6.2886"},
    {"Castellón", "39.9864", "-0.0513"},
    {"Ciudad Real", "38.9848", "-3.9273"},
    {"Córdoba", "37.8882", "-4.7794"},
    {"Cuenca", "40.0704", "-2.1374"},
    {"Girona", "41.9794", "2.8214"},
    {"Granada", "37.1773", "-3.5986"},
    {"Guadalajara", "40.6320", "-3.1608"},
    {"Huelva", "37.2614", "-6.9447"},
    {"Huesca", "42.1362", "-0.4087"},
    {"Jaén", "37.7796", "-3.7849"},
    {"León", "42.5987", "-5.5671"},
    {"Lleida", "41.6176", "0.6200"},
    {"Logroño", "42.4627", "-2.4449"},
    {"Lugo", "43.0121", "-7.5559"},
    {"Madrid", "40.4168", "-3.7038"},
    {"Málaga", "36.7213", "-4.4214"},
    {"Murcia", "37.9922", "-1.1307"},
    {"Ourense", "42.3358", "-7.8639"},
    {"Oviedo", "43.3619", "-5.8494"},
    {"Palencia", "42.0096", "-4.5288"},
    {"Palma", "39.5696", "2.6502"},
    {"Las Palmas", "28.1235", "-15.4363"},
    {"Pamplona", "42.8125", "-1.6458"},
    {"Pontevedra", "42.4310", "-8.6444"},
    {"Salamanca", "40.9701", "-5.6635"},
    {"San Sebastián", "43.3183", "-1.9812"},
    {"Santander", "43.4623", "-3.8100"},
    {"Sta. Cruz Tenerife", "28.4636", "-16.2518"},
    {"Segovia", "40.9429", "-4.1088"},
    {"Sevilla", "37.3891", "-5.9845"},
    {"Soria", "41.7665", "-2.4790"},
    {"Tarragona", "41.1189", "1.2445"},
    {"Teruel", "40.3456", "-1.1065"},
    {"Toledo", "39.8628", "-4.0273"},
    {"Valencia", "39.4699", "-0.3763"},
    {"Valladolid", "41.6523", "-4.7245"},
    {"Vitoria", "42.8467", "-2.6716"},
    {"Zamora", "41.5033", "-5.7446"},
    {"Zaragoza", "41.6488", "-0.8891"},
};

// Try silent WiFi connect using saved credentials (no UI)
static bool trySilentWifiConnect() {
  OpportunisticTimeSync::claimForeground();
  const auto& ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) return false;

  const auto* cred = WIFI_STORE.findCredential(ssid);
  if (!cred) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(cred->ssid.c_str(), cred->password.c_str());

  // Wait up to 10 seconds for connection
  for (int i = 0; i < 100; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  WiFi.disconnect(false);
  return false;
}

void WeatherActivity::onEnter() {
  Activity::onEnter();
  selectedCity = SETTINGS.weatherCity;
  if (selectedCity > CITY_COUNT) selectedCity = 0;  // 0=Auto, 1..3=manual
  lastUpdateTime[0] = '\0';
  detectedCityName[0] = '\0';
  detectedLat[0] = '\0';
  detectedLon[0] = '\0';

  state = WIFI_CONNECTING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    onWifiConnected();
  } else if (trySilentWifiConnect()) {
    onWifiConnected();
  } else {
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
        [this](const ActivityResult& r) {
          if (r.isCancelled) {
            finish();
            return;
          }
          onWifiConnected();
        });
  }
}

void WeatherActivity::onWifiConnected() {
  // Auto-detect location by IP if selectedCity == 0 (also detects timezone offset)
  if (selectedCity == 0) {
    state = FETCHING;
    statusMessage = tr(STR_FETCHING_WEATHER);
    requestUpdate(true);
    if (!detectLocation()) {
      // Fallback to first manual city if geolocation fails
      LOG_ERR("WEATHER", "IP geolocation failed, falling back to manual city");
      selectedCity = 1;
    }
  }

  // Kick off an NTP sync with the configured timezone (skip in manual clock mode)
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_NTP) {
    const char* tz = getenv("TZ");
    configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
  }

  state = FETCHING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);
  fetchWeather();
}

bool WeatherActivity::detectLocation() {
  std::string response;
  if (!HttpDownloader::fetchUrl("http://ip-api.com/json/?fields=lat,lon,city", response)) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;

  float lat = doc["lat"] | 0.0f;
  float lon = doc["lon"] | 0.0f;
  const char* city = doc["city"] | "";

  if (lat == 0.0f && lon == 0.0f) return false;

  snprintf(detectedLat, sizeof(detectedLat), "%.4f", lat);
  snprintf(detectedLon, sizeof(detectedLon), "%.4f", lon);
  strncpy(detectedCityName, city, sizeof(detectedCityName) - 1);
  detectedCityName[sizeof(detectedCityName) - 1] = '\0';

  LOG_DBG("WEATHER", "IP geolocation: %s (%.4f, %.4f)", detectedCityName, lat, lon);
  return true;
}

const char* WeatherActivity::getCurrentCityName() const {
  if (selectedCity == 0) {
    return detectedCityName[0] ? detectedCityName : "Auto";
  }
  return CITIES[selectedCity - 1].name;
}

void WeatherActivity::fetchWeather() {
  const char* lat;
  const char* lon;

  if (selectedCity == 0 && detectedLat[0]) {
    lat = detectedLat;
    lon = detectedLon;
  } else {
    int idx = (selectedCity > 0) ? selectedCity - 1 : 0;
    lat = CITIES[idx].lat;
    lon = CITIES[idx].lon;
  }

  char url[384];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,"
           "apparent_temperature,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&forecast_days=6",
           lat, lon);

  std::string response;
  if (!HttpDownloader::fetchUrl(std::string(url), response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_WEATHER_ERROR);
    requestUpdate(true);
    return;
  }

  if (!parseWeather(response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_WEATHER_ERROR);
    requestUpdate(true);
    return;
  }

  time_t now;
  time(&now);
  struct tm ti;
  localtime_r(&now, &ti);
  snprintf(lastUpdateTime, sizeof(lastUpdateTime), "%02d:%02d", ti.tm_hour, ti.tm_min);

  saveWeatherCache();

  state = DISPLAYING;
  requestUpdate(true);
}

bool WeatherActivity::parseWeather(const std::string& json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("WEATHER", "JSON parse error: %s", error.c_str());
    return false;
  }

  JsonObject current = doc["current"];
  if (current.isNull()) return false;

  weather.temperature = current["temperature_2m"] | 0.0f;
  weather.feelsLike = current["apparent_temperature"] | 0.0f;
  weather.humidity = current["relative_humidity_2m"] | 0;
  weather.weatherCode = current["weather_code"] | 0;
  weather.windSpeed = current["wind_speed_10m"] | 0.0f;

  // Parse daily forecast (skip day 0 = today, show next 5 days)
  forecastCount = 0;
  JsonObject daily = doc["daily"];
  if (!daily.isNull()) {
    static const char* DAY_NAMES[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    JsonArray codes = daily["weather_code"];
    JsonArray maxTemps = daily["temperature_2m_max"];
    JsonArray minTemps = daily["temperature_2m_min"];
    JsonArray dates = daily["time"];
    for (int i = 1; i < (int)codes.size() && forecastCount < FORECAST_DAYS; i++) {
      auto& f = forecast[forecastCount];
      f.weatherCode = codes[i] | 0;
      f.tempMax = maxTemps[i] | 0.0f;
      f.tempMin = minTemps[i] | 0.0f;
      // Parse day of week from date string "YYYY-MM-DD"
      const char* dateStr = dates[i] | "";
      struct tm tm = {};
      if (sscanf(dateStr, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_year -= 1900; tm.tm_mon -= 1;
        mktime(&tm);
        strncpy(f.dayLabel, DAY_NAMES[tm.tm_wday], sizeof(f.dayLabel) - 1);
      }
      forecastCount++;
    }
  }
  return true;
}

const char* WeatherActivity::weatherCodeToString(int code) {
  if (code == 0) return tr(STR_WEATHER_CLEAR);
  if (code <= 3) return tr(STR_WEATHER_PARTLY_CLOUDY);
  if (code <= 48) return tr(STR_WEATHER_FOG);
  if (code <= 57) return tr(STR_WEATHER_DRIZZLE);
  if (code <= 67) return tr(STR_WEATHER_RAIN);
  if (code <= 77) return tr(STR_WEATHER_SNOW);
  if (code <= 82) return tr(STR_WEATHER_SHOWERS);
  return tr(STR_WEATHER_THUNDERSTORM);
}

void WeatherActivity::saveWeatherCache() {
  JsonDocument doc;
  doc["temp"] = weather.temperature;
  doc["feels"] = weather.feelsLike;
  doc["hum"] = weather.humidity;
  doc["code"] = weather.weatherCode;
  doc["wind"] = weather.windSpeed;
  doc["city"] = selectedCity;
  doc["time"] = lastUpdateTime;
  if (detectedCityName[0]) doc["autoCity"] = detectedCityName;
  if (detectedLat[0]) {
    doc["autoLat"] = detectedLat;
    doc["autoLon"] = detectedLon;
  }

  String json;
  serializeJson(doc, json);
  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.writeFile("/.crosspoint/weather_cache.json", json);
}

bool WeatherActivity::loadWeatherCache(WeatherData& out, uint8_t& cityIdx, char* timeBuf, size_t timeBufLen,
                                       char* autoCityBuf, size_t autoCityBufLen) {
  String content = Storage.readFile("/.crosspoint/weather_cache.json");
  if (content.isEmpty()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, content)) return false;

  out.temperature = doc["temp"] | 0.0f;
  out.feelsLike = doc["feels"] | 0.0f;
  out.humidity = doc["hum"] | 0;
  out.weatherCode = doc["code"] | 0;
  out.windSpeed = doc["wind"] | 0.0f;
  cityIdx = doc["city"] | (uint8_t)0;
  if (cityIdx > CITY_COUNT) cityIdx = 0;

  const char* t = doc["time"] | "";
  strncpy(timeBuf, t, timeBufLen - 1);
  timeBuf[timeBufLen - 1] = '\0';

  if (autoCityBuf && autoCityBufLen > 0) {
    const char* ac = doc["autoCity"] | "";
    strncpy(autoCityBuf, ac, autoCityBufLen - 1);
    autoCityBuf[autoCityBufLen - 1] = '\0';
  }
  return true;
}

int WeatherActivity::silentRefresh() {
  // Returns: 0=ok, 1=no saved wifi creds, 2=wifi connect timeout, 4=api fail, 5=parse fail
  if (WiFi.status() != WL_CONNECTED) {
    const auto& ssid = WIFI_STORE.getLastConnectedSsid();
    if (ssid.empty()) {
      LOG_ERR("SYNC", "No saved WiFi credentials (empty lastConnectedSsid)");
      return 1;
    }
    const auto* cred = WIFI_STORE.findCredential(ssid);
    if (!cred) {
      LOG_ERR("SYNC", "Credential not found for SSID: %s", ssid.c_str());
      return 1;
    }
    LOG_INF("SYNC", "Connecting to WiFi: %s", cred->ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
    bool connected = false;
    for (int i = 0; i < 100; i++) {
      if (WiFi.status() == WL_CONNECTED) { connected = true; break; }
      delay(100);
    }
    if (!connected) {
      LOG_ERR("SYNC", "WiFi connect timeout (10s)");
      WiFi.disconnect(false); WiFi.mode(WIFI_OFF); return 2;
    }
    LOG_INF("SYNC", "WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
    delay(500);  // Allow DNS resolver to initialize after WiFi connect
  }

  // Weather fetch removed — the weather feature is no longer used. This "sync"
  // now only refreshes the clock over NTP. WiFi is already connected at this
  // point (above); bring it up, sync time, tear back down.

  // NTP sync with the configured timezone (skip in manual clock mode)
  if (SETTINGS.clockMode == CrossPointSettings::CLOCK_NTP) {
    const char* tz = getenv("TZ");
    configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
    // Wait for NTP sync (up to 8 seconds)
    bool got = false;
    for (int i = 0; i < 80; i++) {
      if (time(nullptr) > 1700000000) { got = true; break; }
      delay(100);
    }
    WiFi.disconnect(false); delay(100); WiFi.mode(WIFI_OFF);
    return got ? 0 : 2;  // 0=ok, 2=timeout
  }

  // Manual clock mode: nothing to sync.
  WiFi.disconnect(false); delay(100); WiFi.mode(WIFI_OFF);
  return 0;
}

void WeatherActivity::onExit() {
  SETTINGS.weatherCity = selectedCity;
  SETTINGS.saveToFile();
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  Activity::onExit();
}

void WeatherActivity::loop() {
  auto back = mappedInput.wasReleased(MappedInputManager::Button::Back);
  auto confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  auto next = mappedInput.wasReleased(MappedInputManager::Button::Right);
  auto prev = mappedInput.wasReleased(MappedInputManager::Button::Left);
  auto up = mappedInput.wasReleased(MappedInputManager::Button::Up);
  auto down = mappedInput.wasReleased(MappedInputManager::Button::Down);

  if (state == SELECTING_CITY) {
    int total = CITY_COUNT + 1;  // 0=Auto + 63 cities
    if (back) {
      state = DISPLAYING;
      requestUpdate();
      return;
    }
    if (down || next) {
      cityCursor = (cityCursor + 1) % total;
      // Keep cursor visible in scroll window
      if (cityCursor < cityScrollTop) cityScrollTop = cityCursor;
      if (cityCursor >= cityScrollTop + 10) cityScrollTop = cityCursor - 9;
      requestUpdate();
    }
    if (up || prev) {
      cityCursor = (cityCursor + total - 1) % total;
      if (cityCursor < cityScrollTop) cityScrollTop = cityCursor;
      if (cityCursor >= cityScrollTop + 10) cityScrollTop = cityCursor - 9;
      requestUpdate();
    }
    if (confirm) {
      selectedCity = cityCursor;
      if (selectedCity == 0 && detectedLat[0] == '\0') detectLocation();
      state = FETCHING;
      statusMessage = tr(STR_FETCHING_WEATHER);
      requestUpdate(true);
      fetchWeather();
    }
    return;
  }

  if (back) {
    finish();
    return;
  }

  if (state == DISPLAYING || state == FETCH_ERROR) {
    if (confirm) {
      state = FETCHING;
      statusMessage = tr(STR_FETCHING_WEATHER);
      requestUpdate(true);
      fetchWeather();
      return;
    }
    if (next || prev) {
      // Open city selection list
      cityCursor = selectedCity;
      cityScrollTop = (cityCursor > 5) ? cityCursor - 5 : 0;
      state = SELECTING_CITY;
      requestUpdate();
    }
  }
}

void WeatherActivity::renderCityList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CITY));

  int y = metrics.topPadding + metrics.headerHeight + 10;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int total = CITY_COUNT + 1;
  const int visibleCount = 10;

  for (int i = 0; i < visibleCount && (cityScrollTop + i) < total; i++) {
    int idx = cityScrollTop + i;
    const char* name = (idx == 0) ? "Auto" : CITIES[idx - 1].name;

    if (idx == cityCursor) {
      // Highlight selected row
      renderer.fillRect(5, y - 2, pageWidth - 10, lineH, true);
      renderer.drawText(UI_10_FONT_ID, 15, y, name, false);  // white text on black
      if (idx == selectedCity) {
        renderer.drawText(UI_10_FONT_ID, pageWidth - 40, y, "*", false);
      }
    } else {
      renderer.drawText(UI_10_FONT_ID, 15, y, name, true);
      if (idx == selectedCity) {
        renderer.drawText(UI_10_FONT_ID, pageWidth - 40, y, "*", true);
      }
    }
    y += lineH;
  }

  // Scroll indicators
  if (cityScrollTop > 0) {
    renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding + metrics.headerHeight, "▲");
  }
  if (cityScrollTop + visibleCount < total) {
    renderer.drawCenteredText(SMALL_FONT_ID, y + 2, "▼");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

// Draw a 32x32 icon at 1x using drawPixel (orientation-aware, unlike drawIcon)
static void drawIcon1x(GfxRenderer& r, const uint8_t* icon, int x, int y) {
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      const int byteIdx = row * 4 + col / 8;
      const int bitIdx = 7 - (col % 8);
      if (!((icon[byteIdx] >> bitIdx) & 1)) {
        r.drawPixel(x + col, y + row);
      }
    }
  }
}

// Draw a 32x32 icon scaled 2x (64x64) using pixel doubling
static void drawIconScaled2x(GfxRenderer& r, const uint8_t* icon, int x, int y) {
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      int byteIdx = row * 4 + col / 8;
      int bitIdx = 7 - (col % 8);
      bool isBlack = !((icon[byteIdx] >> bitIdx) & 1);
      if (isBlack) {
        int dx = x + col * 2;
        int dy = y + row * 2;
        r.drawPixel(dx, dy);
        r.drawPixel(dx + 1, dy);
        r.drawPixel(dx, dy + 1);
        r.drawPixel(dx + 1, dy + 1);
      }
    }
  }
}

void WeatherActivity::render(RenderLock&&) {
  if (state == SELECTING_CITY) {
    renderCityList();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();

  if (state == FETCHING || state == WIFI_CONNECTING || state == FETCH_ERROR) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WEATHER));
    int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, statusMessage.c_str());
  } else {
    char buf[64];
    int y = metrics.topPadding + 16;

    // City name — centered bold
    renderer.drawCenteredText(UI_12_FONT_ID, y, getCurrentCityName(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 2;

    // Weather description
    const char* desc = weatherCodeToString(weather.weatherCode);
    renderer.drawCenteredText(UI_10_FONT_ID, y, desc);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 12;

    // Large weather icon (64x64 via 2x scaling) — centered
    constexpr int ICON_DISPLAY_SIZE = 64;
    const uint8_t* icon = getWeatherIcon(weather.weatherCode);
    drawIconScaled2x(renderer, icon, (pageWidth - ICON_DISPLAY_SIZE) / 2, y);
    y += ICON_DISPLAY_SIZE + 8;

    // Large temperature
    snprintf(buf, sizeof(buf), "%.0f%s", convertTemp(weather.temperature), tempUnitSuffix());
    renderer.drawCenteredText(LEXEND_18_FONT_ID, y, buf, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(LEXEND_18_FONT_ID) + 4;

    // Feels like
    snprintf(buf, sizeof(buf), "%s %.0f%s", tr(STR_FEELS_LIKE), convertTemp(weather.feelsLike), tempUnitSuffix());
    renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 16;

    // Details card — humidity & wind in rounded rect
    const int cardX = 20;
    const int cardW = pageWidth - 40;
    const int cardH = 80;
    renderer.drawRoundedRect(cardX, y, cardW, cardH, 1, 8, true);
    const int colW = cardW / 2;
    const int detailY = y + 12;

    // Humidity
    snprintf(buf, sizeof(buf), "%s", tr(STR_HUMIDITY));
    int tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
    renderer.drawText(SMALL_FONT_ID, cardX + (colW - tw) / 2, detailY, buf);
    snprintf(buf, sizeof(buf), "%d%%", weather.humidity);
    tw = renderer.getTextWidth(UI_12_FONT_ID, buf);
    renderer.drawText(UI_12_FONT_ID, cardX + (colW - tw) / 2, detailY + 26, buf, true, EpdFontFamily::BOLD);

    // Wind
    snprintf(buf, sizeof(buf), "%s", tr(STR_WIND));
    tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
    renderer.drawText(SMALL_FONT_ID, cardX + colW + (colW - tw) / 2, detailY, buf);
    snprintf(buf, sizeof(buf), "%.1f km/h", weather.windSpeed);
    tw = renderer.getTextWidth(UI_12_FONT_ID, buf);
    renderer.drawText(UI_12_FONT_ID, cardX + colW + (colW - tw) / 2, detailY + 26, buf, true, EpdFontFamily::BOLD);

    renderer.drawLine(cardX + colW, y + 8, cardX + colW, y + cardH - 8, true);
    y += cardH + 12;

    // 5-day forecast row
    if (forecastCount > 0) {
      renderer.drawLine(cardX, y, cardX + cardW, y, true);
      y += 8;
      const int cols = forecastCount;
      const int cellW = cardW / cols;
      for (int i = 0; i < cols; i++) {
        const auto& f = forecast[i];
        int cx = cardX + i * cellW + cellW / 2;

        // Day label centered
        tw = renderer.getTextWidth(SMALL_FONT_ID, f.dayLabel);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y, f.dayLabel);

        // Small icon centered (32x32)
        const uint8_t* fIcon = getWeatherIcon(f.weatherCode);
        drawIcon1x(renderer, fIcon, cx - WEATHER_ICON_SIZE / 2, y + 20);

        // High/Low temp
        snprintf(buf, sizeof(buf), "%.0f%s", convertTemp(f.tempMax), tempUnitSuffix());
        tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y + 56, buf, true, EpdFontFamily::BOLD);

        snprintf(buf, sizeof(buf), "%.0f%s", convertTemp(f.tempMin), tempUnitSuffix());
        tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y + 74, buf);
      }
      y += 96;
    }

    // Last updated
    if (lastUpdateTime[0]) {
      snprintf(buf, sizeof(buf), "%s: %s", tr(STR_LAST_UPDATED), lastUpdateTime);
      renderer.drawCenteredText(SMALL_FONT_ID, y, buf);
    }
  }

  // Button hints: Back | Refresh | Location (Left+Right both open city picker)
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_LOCATION), tr(STR_LOCATION));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
