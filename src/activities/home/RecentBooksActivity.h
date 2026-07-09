#pragma once
#include <I18n.h>

#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

// Library screen: shows *every* book found on the SD card as a cover grid
// (3 columns) with the title below each cover. Covers already cached on disk
// are shown; books without a cached cover fall back to a titled placeholder
// card. Navigate with the D-pad, Confirm opens the book, Back goes home.
class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

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

  void loadLibraryBooks();
  void scanDir(const std::string& dirPath, int depth);
  int totalPages() const;
  void ensurePageForIndex();

  // Per-card derived metadata (cheap; no EPUB load).
  std::string bookTitle(const std::string& path) const;
  std::string coverThumbFor(const std::string& path) const;
  uint8_t progressFor(const std::string& path) const;
  const RecentBook* recentLookup(const std::string& path) const;

  // Render a single cover card at a grid position.
  void renderCard(int bookIdx, int gridCol, int gridRow, int cardW, int cardH, int startX, int startY, bool selected);

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
