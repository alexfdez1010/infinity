#pragma once

#include <cstdint>

#include "../Activity.h"

// "Desatasco" — a Rush Hour / Unblock Me style sliding-block puzzle for e-ink.
// A 6x6 grid is packed with horizontal and vertical pieces (length 2 or 3).
// Each piece slides only along its own orientation (horizontal pieces left/
// right, vertical pieces up/down), one cell per press, blocked by walls and by
// other pieces. The target piece (index 0) is a horizontal 2-cell block sitting
// on the exit row; the goal is to slide it right until it reaches the exit gap
// on the right edge.
//
// Pure deduction, no timing — a good fit for slow e-ink refresh and the
// D-pad + Confirm + Back layout, in the same spirit as the sliding puzzle.
//
// Boards are generated at random and graded by difficulty: a breadth-first
// solver verifies every board is solvable and measures its minimum solution
// length, so the puzzle handed out always has a solution and matches the chosen
// difficulty band. Generation is bounded (attempt/state caps) so it can never
// hang, and falls back to a small hardcoded solvable board if needed.
//
// Controls: in SELECT, Up/Down pick a difficulty, Confirm starts, Back exits.
// In PLAY, Confirm cycles the selected piece, the D-pad slides it, Back returns
// to difficulty selection. Winning shows a card; Confirm then deals a new board.
class BlocksActivity final : public Activity {
  static constexpr int BOARD = 6;       // 6x6 grid
  static constexpr int EXIT_ROW = 2;    // exit gap sits on this row, right edge
  static constexpr int MAX_PIECES = 14;  // fits 3 bits/piece into a uint64 key

  // A rectangular block. (row,col) is its top-left cell; horizontal pieces span
  // cols [col, col+len), vertical pieces span rows [row, row+len).
  struct Piece {
    uint8_t row;
    uint8_t col;
    uint8_t len;       // 2 or 3
    bool horizontal;
  };

  enum class State { SELECT, GENERATING, PLAY, WON };

  State state = State::SELECT;
  int difficulty = 0;      // 0 = easy, 1 = medium, 2 = hard
  int menuSel = 0;         // highlighted option on the SELECT screen

  Piece pieces[MAX_PIECES];
  int pieceCount = 0;
  int selectedPiece = 0;   // piece the D-pad currently drives (PLAY)
  uint32_t moves = 0;

  // Fill an occupancy grid (cell -> piece index, or -1 for empty) from pieces[].
  void buildGrid(int8_t grid[BOARD][BOARD]) const;
  // Try to slide piece idx by (dRow,dCol) one cell; honours orientation, walls
  // and other pieces. Returns true if the piece actually moved.
  bool tryMove(int idx, int dRow, int dCol);
  // True when the target piece (index 0) has reached the exit on the right edge.
  bool targetEscaped() const;

  // Generate a fresh random layout for the given difficulty into pieces[].
  void generate(int diff);
  // Overwrite pieces[] with a tiny hardcoded solvable board (last resort).
  void loadFallback(int diff);
  // Build a solvable, difficulty-graded board (retrying + solving). Never hangs.
  void newPuzzle();
  // Show the "generating" screen, then build a fresh board. Board generation can
  // take a moment (repeated BFS solves), so this flushes a loading frame first.
  void startNewPuzzle();
  // Breadth-first search over board states: minimum number of piece-moves to
  // free the target, or -1 if unsolvable within the bounded state cap.
  int solve() const;

  void recordBest();

 public:
  explicit BlocksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Blocks", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
