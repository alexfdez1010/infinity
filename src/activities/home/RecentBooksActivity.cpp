#include "RecentBooksActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Xtc.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Must match the home screen's CP_COVER_H so we reuse the same cached thumbnails
constexpr int THUMB_H = 188;
// Guardrails for the recursive SD scan (bounded heap + no runaway on huge trees)
constexpr int MAX_SCAN_DEPTH = 8;
constexpr size_t MAX_BOOKS = 300;
constexpr char CACHE_DIR[] = "/.crosspoint";

bool isBookFile(std::string_view name) {
  return FsHelpers::hasEpubExtension(name) || FsHelpers::hasXtcExtension(name) ||
         FsHelpers::hasTxtExtension(name) || FsHelpers::hasMarkdownExtension(name);
}

// Case-insensitive comparison of two UTF-8 strings (ASCII-fold only; good
// enough for ordering a book list on-device).
bool ciLess(const std::string& a, const std::string& b) {
  size_t i = 0;
  const size_t n = std::min(a.size(), b.size());
  for (; i < n; i++) {
    const char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
    const char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
    if (c1 != c2) return c1 < c2;
  }
  return a.size() < b.size();
}

// Split text into up to maxLines that each fit maxWidth; the last line is
// ellipsised if the text overflows.
std::vector<std::string> wrapLines(GfxRenderer& renderer, int fontId, const std::string& text, int maxWidth,
                                   int maxLines) {
  std::vector<std::string> lines;
  std::string remaining = text;
  for (int line = 0; line < maxLines && !remaining.empty(); line++) {
    const bool lastLine = (line == maxLines - 1);
    if (lastLine || renderer.getTextWidth(fontId, remaining.c_str()) <= maxWidth) {
      // Whole (or truncated) remainder on the final line.
      lines.push_back(lastLine ? renderer.truncatedText(fontId, remaining.c_str(), maxWidth) : remaining);
      remaining.clear();
      break;
    }
    // Find the last word boundary that still fits.
    size_t breakAt = std::string::npos;
    for (size_t i = 0; i < remaining.size(); i++) {
      if (remaining[i] != ' ') continue;
      if (renderer.getTextWidth(fontId, remaining.substr(0, i).c_str()) <= maxWidth) {
        breakAt = i;
      } else {
        break;
      }
    }
    if (breakAt == std::string::npos) {
      // A single word wider than the line — hard-truncate it onto this line.
      lines.push_back(renderer.truncatedText(fontId, remaining.c_str(), maxWidth));
      remaining.clear();
      break;
    }
    lines.push_back(remaining.substr(0, breakAt));
    remaining.erase(0, breakAt + 1);
  }
  return lines;
}
}  // namespace

const RecentBook* RecentBooksActivity::recentLookup(const std::string& path) const {
  const auto& books = RECENT_BOOKS.getBooks();
  auto it = std::find_if(books.begin(), books.end(), [&](const RecentBook& b) { return b.path == path; });
  return it == books.end() ? nullptr : &*it;
}

std::string RecentBooksActivity::bookTitle(const std::string& path) const {
  if (const RecentBook* r = recentLookup(path); r && !r->title.empty()) {
    return r->title;
  }
  // Derive a readable title from the file name (strip directory + extension).
  std::string name = path;
  const size_t slash = name.find_last_of('/');
  if (slash != std::string::npos) name = name.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot != 0) name = name.substr(0, dot);
  return name;
}

std::string RecentBooksActivity::coverThumbFor(const std::string& path) const {
  if (FsHelpers::hasEpubExtension(path)) {
    return Epub(path, CACHE_DIR).getThumbBmpPath(THUMB_H);
  }
  if (FsHelpers::hasXtcExtension(path)) {
    return Xtc(path, CACHE_DIR).getThumbBmpPath(THUMB_H);
  }
  return "";
}

uint8_t RecentBooksActivity::progressFor(const std::string& path) const {
  const RecentBook* r = recentLookup(path);
  return r ? r->progressPercent : 0;
}

void RecentBooksActivity::scanDir(const std::string& dirPath, int depth) {
  if (depth > MAX_SCAN_DEPTH || bookPaths.size() >= MAX_BOOKS) return;

  auto dir = Storage.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) return;
  dir.rewindDirectory();

  char name[500];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (bookPaths.size() >= MAX_BOOKS) break;
    file.getName(name, sizeof(name));

    // Skip hidden entries (incl. the /.crosspoint cache) and the Windows folder.
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) continue;

    std::string childPath = dirPath;
    if (childPath.back() != '/') childPath += '/';
    childPath += name;

    if (file.isDirectory()) {
      scanDir(childPath, depth + 1);
    } else if (isBookFile(name)) {
      bookPaths.push_back(childPath);
    }
  }
}

void RecentBooksActivity::loadLibraryBooks() {
  bookPaths.clear();
  scanDir("/", 0);

  // Sort by displayed title (case-insensitive) for a predictable shelf order.
  std::vector<std::pair<std::string, std::string>> keyed;  // (display title, path)
  keyed.reserve(bookPaths.size());
  for (auto& path : bookPaths) {
    keyed.emplace_back(bookTitle(path), std::move(path));
  }
  std::sort(keyed.begin(), keyed.end(),
            [](const auto& a, const auto& b) { return ciLess(a.first, b.first); });

  bookPaths.clear();
  bookPaths.reserve(keyed.size());
  for (auto& kv : keyed) bookPaths.push_back(std::move(kv.second));
}

int RecentBooksActivity::totalPages() const {
  if (bookPaths.empty() || itemsPerPage <= 0) return 1;
  return (static_cast<int>(bookPaths.size()) + itemsPerPage - 1) / itemsPerPage;
}

void RecentBooksActivity::ensurePageForIndex() {
  if (itemsPerPage <= 0) return;
  const int page = selectorIndex / itemsPerPage;
  pageOffset = page * itemsPerPage;
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();
  loadLibraryBooks();
  selectorIndex = 0;
  pageOffset = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  bookPaths.clear();
}

void RecentBooksActivity::loop() {
  const int total = static_cast<int>(bookPaths.size());
  if (total == 0) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  buttonNavigator.onNext([this, total] {
    selectorIndex = (selectorIndex + 1) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, total] {
    selectorIndex = (selectorIndex - 1 + total) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex < total) {
      onSelectBook(bookPaths[selectorIndex]);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }
}

void RecentBooksActivity::renderCard(int bookIdx, int gridCol, int gridRow, int cardW, int cardH, int startX,
                                     int startY, bool selected) {
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int titleAreaH = TITLE_LINES * lineH + 4;
  const int rowH = cardH + titleAreaH + 12;

  const int cx = startX + gridCol * (cardW + COVER_GAP);
  const int cy = startY + gridRow * rowH;

  const std::string& path = bookPaths[bookIdx];
  const std::string thumbPath = coverThumbFor(path);
  const bool hasCover = !thumbPath.empty() && Storage.exists(thumbPath.c_str());

  // Soft drop shadow for a touch of depth (skipped when selected — the bold
  // frame already lifts the card off the page).
  if (!selected) {
    renderer.fillRoundedRect(cx + 2, cy + 3, cardW, cardH, COVER_R, Color::LightGray);
  }

  // Card surface.
  renderer.fillRoundedRect(cx, cy, cardW, cardH, COVER_R, Color::White);

  if (hasCover) {
    FsFile f;
    if (Storage.openFileForRead("RBA", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        const int bmpW = std::min((int)bmp.getWidth(), cardW - 4);
        const int bmpH = std::min((int)bmp.getHeight(), cardH - 4);
        renderer.drawBitmap(bmp, cx + (cardW - bmpW) / 2, cy + (cardH - bmpH) / 2, bmpW, bmpH);
      }
      f.close();
    }
  } else {
    // No cached cover: a titled placeholder that still reads as a book spine.
    renderer.fillRoundedRect(cx, cy, cardW, cardH, COVER_R, Color::LightGray);
    renderer.fillRect(cx + 7, cy + 8, 4, cardH - 16, true);  // spine accent

    const auto lines = wrapLines(renderer, UI_10_FONT_ID, bookTitle(path), cardW - 22, 3);
    const int block = static_cast<int>(lines.size()) * renderer.getLineHeight(UI_10_FONT_ID);
    int ty = cy + (cardH - block) / 2 + renderer.getLineHeight(UI_10_FONT_ID) - 4;
    for (const auto& l : lines) {
      const int lw = renderer.getTextWidth(UI_10_FONT_ID, l.c_str());
      renderer.drawText(UI_10_FONT_ID, cx + 14 + ((cardW - 22) - lw) / 2, ty, l.c_str(), true);
      ty += renderer.getLineHeight(UI_10_FONT_ID);
    }
  }

  // Progress bar (only when there is progress to show).
  const uint8_t progress = progressFor(path);
  if (progress > 0) {
    const int barH = 5;
    const int barY = cy + cardH - barH - 4;
    const int barX = cx + 6;
    const int barW = cardW - 12;
    renderer.fillRoundedRect(barX, barY, barW, barH, barH / 2, Color::LightGray);
    const int fillW = barW * progress / 100;
    if (fillW > 2) renderer.fillRoundedRect(barX, barY, fillW, barH, barH / 2, Color::Black);
  }

  // Card frame — bold when selected.
  renderer.drawRoundedRect(cx, cy, cardW, cardH, selected ? 3 : 1, COVER_R, true);

  // Title below the card (up to TITLE_LINES lines, centered, bold when selected).
  const auto titleStyle = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  const auto titleLines = wrapLines(renderer, SMALL_FONT_ID, bookTitle(path), cardW - 2, TITLE_LINES);
  int titleY = cy + cardH + lineH;
  for (const auto& l : titleLines) {
    const int lw = renderer.getTextWidth(SMALL_FONT_ID, l.c_str(), titleStyle);
    renderer.drawText(SMALL_FONT_ID, cx + (cardW - lw) / 2, titleY, l.c_str(), true, titleStyle);
    titleY += lineH;
  }
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Header shows the library size so the user knows the whole shelf is here.
  std::string header = tr(STR_MENU_RECENT_BOOKS);
  if (!bookPaths.empty()) {
    header += " (" + std::to_string(bookPaths.size()) + ")";
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (bookPaths.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    // Grid layout, horizontally centered within the content area.
    const int sidePad = 16;
    const int areaW = pageWidth - 2 * sidePad;
    const int totalGapW = COVER_GAP * (COLS - 1);
    const int cardW = (areaW - totalGapW) / COLS;
    const int cardH = (int)(cardW * 1.4f);

    const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
    const int titleAreaH = TITLE_LINES * lineH + 4;
    const int rowH = cardH + titleAreaH + 12;
    const int rows = std::max(1, contentHeight / rowH);
    itemsPerPage = std::max(1, rows * COLS);

    // Center the used grid width so the shelf sits balanced on the page.
    const int usedW = COLS * cardW + totalGapW;
    const int startX = (pageWidth - usedW) / 2;
    const int startY = contentTop + 4;

    const int total = static_cast<int>(bookPaths.size());
    const int endIdx = std::min(pageOffset + itemsPerPage, total);

    for (int i = pageOffset; i < endIdx; i++) {
      const int localIdx = i - pageOffset;
      const int col = localIdx % COLS;
      const int row = localIdx / COLS;
      renderCard(i, col, row, cardW, cardH, startX, startY, i == selectorIndex);
    }

    // Centered page indicator when the library spans multiple pages.
    if (totalPages() > 1) {
      char pageStr[16];
      snprintf(pageStr, sizeof(pageStr), "%d / %d", (selectorIndex / itemsPerPage) + 1, totalPages());
      const int pw = renderer.getTextWidth(SMALL_FONT_ID, pageStr);
      renderer.drawText(SMALL_FONT_ID, (pageWidth - pw) / 2,
                        pageHeight - metrics.buttonHintsHeight - 14, pageStr, true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
