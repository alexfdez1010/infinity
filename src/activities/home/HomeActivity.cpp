#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FontDecompressor.h>
#include <FontManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Xtc.h>

extern FontDecompressor fontDecompressor;
#ifdef ENABLE_BLE
#include <BluetoothHIDManager.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "activities/network/WifiSelectionActivity.h"
#include "network/OpportunisticTimeSync.h"
#include "activities/tools/WeatherActivity.h"
#include "WifiCredentialStore.h"
#include <WiFi.h>
#include "util/StringUtils.h"

// ── Buffer management ─────────────────────────────────────────────────────────

bool HomeActivity::storeCoverBuffer() {
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  // Guard: need buffer size + 12KB overhead for heap fragmentation safety
  const size_t bufSize = renderer.getBufferSize();
  if (ESP.getFreeHeap() < bufSize + 12 * 1024) return false;
  freeCoverBuffer();
  coverBuffer = static_cast<uint8_t*>(malloc(bufSize));
  if (!coverBuffer) return false;
  memcpy(coverBuffer, fb, bufSize);
  return true;
}

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Recents, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer) return false;
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  memcpy(fb, coverBuffer, renderer.getBufferSize());
  return true;
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) { free(coverBuffer); coverBuffer = nullptr; }
  coverBufferStored = false;
}

// ── Book loading ──────────────────────────────────────────────────────────────

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  for (const RecentBook& b : RECENT_BOOKS.getBooks()) {
    if (static_cast<int>(recentBooks.size()) >= maxBooks) break;
    if (Storage.exists(b.path.c_str())) recentBooks.push_back(b);
  }
}

bool HomeActivity::coverThumbMissing(const RecentBook& b, int coverHeight) const {
  if (!FsHelpers::hasEpubExtension(b.path) && !FsHelpers::hasXtcExtension(b.path)) return false;
  if (b.coverBmpPath.empty()) return true;
  return !Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, coverHeight).c_str());
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;

#ifdef ENABLE_BLE
  // Skip cover thumbnail generation when a BT HID device is connected. NimBLE + GATT
  // context eats ~65-75KB — decoding JPG/PNG on top fragments the heap and risks OOM.
  // The converter-level heap guards (MIN_FREE_HEAP_FOR_THUMB / _PNG_THUMB) would catch
  // this anyway, but skipping entirely avoids the loading popup flash + SD churn on
  // every home re-entry while the remote is paired.
  if (!BluetoothHIDManager::getInstance().getConnectedDevices().empty()) {
    LOG_INF("HOME", "BT HID connected — skipping recent-book cover thumbnail generation");
    recentsLoaded = true;
    recentsLoading = false;
    return;
  }
#endif

  // One cover per render pass. Loading the whole strip in a single blocking
  // batch froze the home screen while every missing thumb decoded (JPEG/PNG
  // decode + Epub load, ~1-2s each). Instead we resolve exactly ONE book that
  // needs heavy work per call, repaint, then let the next render pass pick up
  // the following one — covers pop in progressively, same as the Libros list
  // (RecentBooksActivity::generateMissingThumb). recentsLoaded stays false until
  // a full scan finds nothing left to do, so the render loop keeps calling us.
  for (RecentBook& book : recentBooks) {
    if (FsHelpers::hasEpubExtension(book.path)) {
      // Fast path: if both cover store entry and on-disk thumb already exist,
      // skip Epub load entirely. Loading Epub allocates ~10-20KB (OPF parse +
      // metadata cache); doing it 4× per home entry on a 46KB-free heap with
      // an external font loaded fragments the heap badly enough to corrupt
      // FreeRTOS mutex storage (xTaskPriorityDisinherit assert at home entry
      // after KOReader sync). Only pay the load cost when recovery is needed.
      if (!book.coverBmpPath.empty()) {
        const std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
        if (Storage.exists(coverPath.c_str())) continue;
      }

      // Heavy work needed (recovery load and/or decode). Attempt each book at
      // most once per session so a failed decode doesn't loop forever.
      if (std::find(recentsAttempted.begin(), recentsAttempted.end(), book.path) != recentsAttempted.end()) {
        continue;
      }
      recentsAttempted.push_back(book.path);

      // Slow path: cover store entry is empty (BUG-009 poisoned) or thumb is
      // missing on disk. Load Epub to derive cover path / regenerate thumb.
      Epub epub(book.path, "/.crosspoint");
      epub.load(false, true);

      const bool recoveredPath = book.coverBmpPath.empty();
      if (recoveredPath) {
        book.coverBmpPath = epub.getThumbBmpPath();
      }
      const std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (Storage.exists(coverPath.c_str())) {
        // Thumb already on disk but the store entry was empty — persist the
        // recovered path so the next home entry takes the fast path instead of
        // re-loading the Epub every time.
        if (recoveredPath) {
          RECENT_BOOKS.updateBook(book.path, book.title, book.author, book.coverBmpPath);
        }
      } else {
        // Free font caches around JPEG decode: external font glyph cache
        // (~34KB) + JPEGDEC working set (~33KB) overflows residual heap on
        // ESP32-C3, causing decode to abort with "no cover" when external
        // font is enabled. Caches are restored before returning.
        const bool wasExtLoaded = FontMgr.isExternalFontEnabled() || FontMgr.isUiFontEnabled();
        if (wasExtLoaded) {
          fontDecompressor.clearCache();
          FontMgr.unloadActiveFonts();
        }
        const bool generated = epub.generateThumbBmp(coverHeight);
        if (wasExtLoaded) {
          FontMgr.reloadActiveFonts();
        }
        if (generated) {
          RECENT_BOOKS.updateBook(book.path, book.title, book.author, book.coverBmpPath);
        }
      }

      // One unit of heavy work done — repaint so this cover appears, then bail.
      // recentsLoading is cleared so the next render pass re-enters for the
      // following book; recentsLoaded stays false until nothing is left.
      recentsLoading = false;
      coverRendered = false;
      requestUpdate();
      return;
    } else if (FsHelpers::hasXtcExtension(book.path)) {
      // Same fast path for XTC: skip load when thumb already cached.
      if (!book.coverBmpPath.empty()) {
        const std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
        if (Storage.exists(coverPath.c_str())) continue;
      }
      if (std::find(recentsAttempted.begin(), recentsAttempted.end(), book.path) != recentsAttempted.end()) {
        continue;
      }
      recentsAttempted.push_back(book.path);

      Xtc xtc(book.path, "/.crosspoint");
      if (xtc.load()) {
        const bool recoveredPath = book.coverBmpPath.empty();
        if (recoveredPath) {
          book.coverBmpPath = xtc.getThumbBmpPath();
        }
        const std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
        if (Storage.exists(coverPath.c_str())) {
          if (recoveredPath) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, book.coverBmpPath);
          }
        } else {
          const bool wasExtLoaded = FontMgr.isExternalFontEnabled() || FontMgr.isUiFontEnabled();
          if (wasExtLoaded) {
            fontDecompressor.clearCache();
            FontMgr.unloadActiveFonts();
          }
          const bool generated = xtc.generateThumbBmp(coverHeight);
          if (wasExtLoaded) {
            FontMgr.reloadActiveFonts();
          }
          if (generated) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, book.coverBmpPath);
          }
        }
      }

      recentsLoading = false;
      coverRendered = false;
      requestUpdate();
      return;
    }
  }

  // Full scan completed with no heavy work left — every cover is cached (or was
  // attempted this session). Stop the render loop from calling us again.
  recentsLoaded = true;
  recentsLoading = false;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  selectorIndex = 0;
  coverRendered = false;
  firstRenderDone = false;
  recentsLoaded = false;
  recentsLoading = false;
  recentsAttempted.clear();
  loadRecentBooks(4);
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();
  freeCoverBuffer();
}

// ── Input / Render dispatchers ────────────────────────────────────────────────

void HomeActivity::loop() {
  if (SETTINGS.uiTheme <= CrossPointSettings::LYRA_3_COVERS)
    loopOriginal();
  else if (SETTINGS.uiTheme == CrossPointSettings::CROSSPET_CLASSIC)
    loopClassic();
  else
    loopCrossPet();
}

void HomeActivity::render(RenderLock&&) {
  if (SETTINGS.uiTheme <= CrossPointSettings::LYRA_3_COVERS)
    renderOriginal();
  else if (SETTINGS.uiTheme == CrossPointSettings::CROSSPET_CLASSIC)
    renderClassic();
  else
    renderCrossPet();
}

// ── Shared render helpers ─────────────────────────────────────────────────────

void HomeActivity::renderHeaderClock() {
  int nextX = 10;

  if (!CROSSPET_SETTINGS.appClock) {
    // Show heap even without clock
    if (SETTINGS.showFreeHeap) {
      char heapBuf[12];
      snprintf(heapBuf, sizeof(heapBuf), "%dKB", ESP.getFreeHeap() / 1024);
      renderer.drawText(SMALL_FONT_ID, nextX, 5, heapBuf, true);
    }
    return;
  }

  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  extern bool g_clockApproximate;
  char buf[8];
  if (timeinfo.tm_year >= 125)
    snprintf(buf, sizeof(buf), "%s%02d:%02d", g_clockApproximate ? "~" : "", timeinfo.tm_hour, timeinfo.tm_min);
  else
    snprintf(buf, sizeof(buf), "--:--");

  const int clockW = renderer.getTextWidth(SMALL_FONT_ID, buf);
  renderer.drawText(SMALL_FONT_ID, nextX, 5, buf);
  nextX += clockW + 6;

  // Developer: show free heap after clock
  if (SETTINGS.showFreeHeap) {
    char heapBuf[12];
    snprintf(heapBuf, sizeof(heapBuf), "%dKB", ESP.getFreeHeap() / 1024);
    renderer.drawText(SMALL_FONT_ID, nextX, 5, heapBuf, true);
    nextX += renderer.getTextWidth(SMALL_FONT_ID, heapBuf) + 6;
  }

  // Weather app removed — only surface transient sync status next to the clock.
  if (weatherRefreshing) {
    renderer.drawText(SMALL_FONT_ID, nextX, 5, "...");
  } else if (syncResultMsg) {
    renderer.drawText(SMALL_FONT_ID, nextX, 5, syncResultMsg);
  }
}

// ── Sync ──────────────────────────────────────────────────────────────────────

void HomeActivity::performSyncAfterWifi() {
  static char syncBuf[24];
  weatherRefreshing = true;
  requestUpdateAndWait();

  // Take the radio away from any in-flight background NTP sync
  OpportunisticTimeSync::claimForeground();
  if (WiFi.status() != WL_CONNECTED) {
    const auto& ssid = WIFI_STORE.getLastConnectedSsid();
    const auto* cred = ssid.empty() ? nullptr : WIFI_STORE.findCredential(ssid);
    if (cred) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
      const unsigned long connectStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 8000) {
        delay(100);
      }
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false);
        WiFi.mode(WIFI_OFF);
        weatherRefreshing = false;
        snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_WIFI_CONN_FAILED));
        syncResultMsg = syncBuf;
        syncResultExpiry = millis() + 3000;
        requestUpdate();
        return;
      }
    }
  }

  int rc = WeatherActivity::silentRefresh();
  weatherRefreshing = false;
  if (rc == 0)      snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_SYNC_OK));
  else if (rc == 2) snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_WIFI_TIMEOUT));
  else              snprintf(syncBuf, sizeof(syncBuf), tr(STR_API_ERROR), rc);
  syncResultMsg = syncBuf;
  syncResultExpiry = millis() + 3000;
  coverRendered = false;
  requestUpdate();
}

void HomeActivity::doSync() {
  static char syncBuf2[24];
  if (WIFI_STORE.getLastConnectedSsid().empty()) {
    snprintf(syncBuf2, sizeof(syncBuf2), "%s", tr(STR_WIFI_CONN_FAILED));
    syncResultMsg = syncBuf2;
    syncResultExpiry = millis() + 3000;
    requestUpdate();
    return;
  }
  performSyncAfterWifi();
}

// ── Actions ───────────────────────────────────────────────────────────────────

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }
void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }
void HomeActivity::onRecentBooksOpen() { activityManager.goToRecentBooks(); }
void HomeActivity::onFileTransferOpen(){ activityManager.goToFileTransfer(); }
void HomeActivity::onSettingsOpen()    { activityManager.goToSettings(); }
void HomeActivity::onToolsOpen()       { activityManager.goToTools(); }
void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
