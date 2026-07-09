#pragma once

#include <cstdint>

#include "../Activity.h"

// "Puzzle deslizante" — the classic 15-puzzle for e-ink.
// A 4x4 board holds tiles 1..15 and one blank. Pressing a D-pad direction
// slides the neighbouring tile into the blank (the tile moves in the pressed
// direction). Goal: order the tiles 1..15 with the blank in the bottom-right.
//
// Pure deduction, no timing — a good fit for slow e-ink refresh and the
// D-pad + Confirm + Back layout, in the same spirit as 2048.
//
// Boards are scrambled from the solved state with random legal moves, which
// guarantees a solution always exists (no unsolvable parity).
//
// Controls: D-pad slides a tile, Confirm starts a new game, Back exits.
class SlidingPuzzleActivity final : public Activity {
  static constexpr int SIZE = 4;

  uint8_t grid[SIZE][SIZE];  // 1..15 = tile, 0 = blank
  int blankR = SIZE - 1;
  int blankL = SIZE - 1;
  uint32_t moves = 0;
  bool solved = false;

  // Slide the tile adjacent to the blank in the given direction into the blank.
  // dr/dc point from the blank towards the tile that moves. Returns true if a
  // tile actually moved.
  bool slide(int dr, int dc);
  // Reset to the solved board, then scramble with random legal moves.
  void newPuzzle();
  bool isSolved() const;
  void recordBestIfSolved();

 public:
  explicit SlidingPuzzleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SlidingPuzzle", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
