#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

// Library screen: shows *every* book found on the SD card as a cover grid
// (3 columns) with the title below each cover. Missing cover thumbnails are
// generated lazily after the page is painted (one book per render pass, so the
// UI stays responsive and covers pop in progressively); books without an
// embedded cover fall back to a titled placeholder card. The rendered page is
// cached in a frame buffer so moving the selector only redraws the highlight
// ring instead of re-decoding every cover BMP from SD.
// Navigate with the D-pad, Confirm opens the book, Back goes home.
class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool markReadTriggered = false;  // Guard against re-firing mark-as-read while Up held

  // Mark the selected book 100% read (long-press Up). Persists, invalidates the
  // page cache so the progress bar repaints, and shows a confirmation toast.
  void markSelectedBookRead();

  // Full paths of every book discovered on the SD card, sorted by title.
  // Only paths are kept in RAM; titles/covers are derived per visible card to
  // keep the heap footprint bounded even with a large library.
  std::vector<std::string> bookPaths;

  static constexpr int COLS = 3;
  static constexpr int COVER_GAP = 14;
  static constexpr int COVER_R = 8;
  static constexpr int TITLE_LINES = 2;

  // Pagination
  int pageOffset = 0;    // first book index on current page
  int itemsPerPage = 6;  // recalculated from screen height each render

  // Frame-buffer cache of the current page (everything except the selection
  // ring). Invalidated on page change and after thumbnail generation.
  uint8_t* pageBuffer = nullptr;
  int cachedPageOffset = -1;

  // Books whose thumbnail generation was attempted this session — prevents
  // retry loops when a cover fails to decode (e.g. low heap).
  std::vector<std::string> thumbAttempted;

  void loadLibraryBooks();
  void scanDir(const std::string& dirPath, int depth);
  int totalPages() const;
  void ensurePageForIndex();

  // Per-card derived metadata (cheap; no EPUB load).
  std::string bookTitle(const std::string& path) const;
  std::string coverThumbFor(const std::string& path) const;
  uint8_t progressFor(const std::string& path) const;
  const RecentBook* recentLookup(const std::string& path) const;

  // Page frame-buffer cache management.
  bool storePageBuffer();
  bool restorePageBuffer();
  void freePageBuffer();

  // Generate the first missing thumbnail on the visible page (at most one per
  // call). Returns true if an attempt was made (page must be re-rendered).
  bool generateMissingThumb();

  // Render a single cover card at a grid position (selection drawn separately).
  void renderCard(int bookIdx, int gridCol, int gridRow, int cardW, int cardH, int startX, int startY);
  // Selection ring around the selected card (drawn over the cached page).
  void renderSelectionRing(int cardW, int cardH, int startX, int startY);

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
