#pragma once

#include <cstdint>

#include "../Activity.h"

// "Senku" — peg solitaire on the classic English cross board (7x7 with the
// four 2x2 corners removed, 33 holes). Every hole starts with a peg except the
// centre. A peg jumps orthogonally over an adjacent peg into the empty hole
// beyond it, removing the jumped peg. Goal: finish with a single peg.
//
// Pure deduction, no timing — a natural fit for e-ink and a D-pad + Confirm
// layout, in the same spirit as the Blackout puzzle.
//
// Controls: D-pad moves the cursor. Confirm picks up the peg under the cursor
// (press again to drop it); with a peg picked up, a D-pad press jumps in that
// direction. Back exits. When no jump remains, Confirm starts a new game.
class PegSolitaireActivity final : public Activity {
  static constexpr int GRID = 7;

  bool valid[GRID][GRID];  // part of the cross board?
  bool peg[GRID][GRID];    // hole occupied?
  int cursorX = 3;
  int cursorY = 3;
  int selX = -1;  // picked-up peg, -1 = none
  int selY = -1;
  uint32_t moves = 0;
  bool gameOver = false;

  bool inBoard(int x, int y) const { return x >= 0 && x < GRID && y >= 0 && y < GRID && valid[y][x]; }
  // Attempt a jump from (x,y) by (dx,dy) in board steps of one cell.
  bool tryJump(int x, int y, int dx, int dy);
  bool hasAnyMove() const;
  int pegsLeft() const;
  void newGame();
  void recordBestIfOver();

 public:
  explicit PegSolitaireActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Senku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
