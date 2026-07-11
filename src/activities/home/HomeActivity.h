#pragma once
#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  // Books whose cover decode was attempted this session (one-per-pass lazy load).
  // Prevents a failed/absent-cover book from being retried on every render pass.
  std::vector<std::string> recentsAttempted;
  bool firstRenderDone = false;
  bool weatherRefreshing = false;  // Show "refreshing" status on screen
  const char* syncResultMsg = nullptr;  // "OK" or "Failed" after sync
  unsigned long syncResultExpiry = 0;   // millis() when to clear message
  bool syncTriggered = false;      // Guard against re-triggering sync while held
  bool markReadTriggered = false;  // Guard against re-firing mark-as-read while Up held
  // Mark the currently selected book as 100% read (long-press Up). No-op when the
  // selection is not a book. Persists progress, refreshes the card, shows a toast.
  void markSelectedBookRead();
  bool hasOpdsServers = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  std::vector<RecentBook> recentBooks;
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onRecentBooksOpen();
  void onFileTransferOpen();
  void onSettingsOpen();
  void onToolsOpen();

  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);
  // True when a cover-capable book (EPUB/XTC) has no usable on-disk thumb at
  // this height — including entries whose store cover path is empty and needs
  // recovery via loadRecentCovers().
  bool coverThumbMissing(const RecentBook& b, int coverHeight) const;

  // Original upstream layout (CLASSIC/LYRA/LYRA_3_COVERS)
  int getMenuItemCount() const;
  void renderOriginal();
  void loopOriginal();

  // CrossPet (new card layout) render helpers
  void renderContinueReadingCard();
  void renderRecentCovers();       // Draw cover thumbnails (cached in buffer)
  void renderRecentSelection();    // Selection highlight for recent covers
  void renderReadingStatsBar();    // Reading stats in gap
  void renderBottomBarIcons();      // Static icons + labels (cached in buffer)
  void renderBottomBarSelection();  // Selection highlight only (per-frame)
  void renderButtonHints();         // Static button hint shapes (cached in buffer)
  void renderSelectionHighlight();
  void renderFocusCard();               // Focus mode: large single-book card

  // CrossPet Classic (v1.6.8 grid layout) render helpers
  void renderCoverPanel(int panelX, int panelY, int panelW, int panelH, int coverH);
  void renderProgressPanel(int panelX, int panelY, int panelW, int panelH);
  void renderGridCell(int cellX, int cellY, int cellW, int cellH,
                      int gridIdx, const uint8_t* icon, const char* label);
  void renderClassicSelectionHighlight(int panelX, int panelY, int panelW, int panelH);

  // Shared render helpers
  void renderHeaderClock();
  void doSync();
  void performSyncAfterWifi();

  // Theme-specific render/loop dispatchers
  void renderCrossPet();
  void renderClassic();
  void loopCrossPet();
  void loopClassic();

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void releaseCaches() override { freeCoverBuffer(); }
};
