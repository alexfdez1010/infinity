#pragma once

#include <cstdint>

#include "../Activity.h"

// "Laberinto" — a perfect-maze navigation puzzle for e-ink.
//
// The player starts at the top-left cell and must reach the bottom-right cell,
// moving one cell at a time with the D-pad. Every maze is a "perfect" maze:
// exactly one path between any two cells, so a solution is always guaranteed.
//
// Pure deduction, no timing — a good fit for slow e-ink refresh and the
// D-pad + Confirm + Back layout, in the same spirit as the sliding puzzle.
//
// Three difficulties change the grid size (columns x rows):
//   Easy 8x6, Medium 12x9, Hard 16x12.
// The high score per difficulty is the fewest steps ever taken to solve it.
//
// Controls:
//   SELECT — Up/Down pick a difficulty, Confirm starts, Back exits.
//   PLAY   — D-pad walks the avatar, Confirm regenerates the maze, Back returns
//            to SELECT.
//   WON    — Confirm makes a new maze, Back returns to SELECT.
class MazeActivity final : public Activity {
  // Fixed-size storage sized for the largest difficulty (Hard, 16x12). Smaller
  // mazes simply use a top-left sub-rectangle. No dynamic allocation.
  static constexpr int MAXC = 16;  // max columns
  static constexpr int MAXR = 12;  // max rows
  static constexpr int MAXCELLS = MAXC * MAXR;

  // Wall bitmask per cell: bit0=N, bit1=E, bit2=S, bit3=W.
  // A set bit means a wall is present on that side of the cell. A perfect maze
  // starts with every wall set and DFS carving clears walls to open passages.
  static constexpr uint8_t WALL_N = 0x01;
  static constexpr uint8_t WALL_E = 0x02;
  static constexpr uint8_t WALL_S = 0x04;
  static constexpr uint8_t WALL_W = 0x08;

  // Internal state machine.
  enum class Mode : uint8_t { SELECT, PLAY, WON };
  Mode mode = Mode::SELECT;

  // Difficulty: 0 = Easy, 1 = Medium, 2 = Hard. Also indexes GAME_SCORES.bestMaze.
  int diff = 0;      // committed difficulty of the current maze
  int selDiff = 0;   // highlighted option on the SELECT screen

  int cols = 8;  // current maze width  (<= MAXC)
  int rows = 6;  // current maze height (<= MAXR)

  uint8_t cellWalls[MAXR][MAXC];  // wall bitmask for each cell

  int playerR = 0;  // avatar position
  int playerC = 0;
  uint32_t steps = 0;  // successful moves in the current maze

  // Fill cols/rows from a difficulty index.
  void applyDifficulty(int d);
  // Build a fresh perfect maze at the current difficulty and reset the avatar.
  void generateMaze();
  // Carve away the wall between two orthogonally-adjacent cells (both sides).
  void carve(int r, int c, int nr, int nc);
  // Try to walk one cell in the given direction; returns true if it moved.
  bool tryMove(int dr, int dc);
  // Record the best (fewest-steps) result for the current difficulty on a win.
  void recordBestIfWon();

 public:
  explicit MazeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Maze", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
