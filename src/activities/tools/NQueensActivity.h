#pragma once

#include <cstdint>

#include "../Activity.h"

// Adaptive N-Queens puzzle. Some queens are fixed; place the rest so no two
// share a row, column or diagonal. Puzzles have exactly one completion.
class NQueensActivity final : public Activity {
  static constexpr int MAX_N = 10;

  int8_t queens[MAX_N]{};   // column per row, -1 when empty
  int8_t givens[MAX_N]{};   // immutable clues
  int8_t solution[MAX_N]{};
  int size = 4;
  int cursorX = 0;
  int cursorY = 0;
  uint32_t puzzleElo = 1400;
  bool solved = false;
  bool generating = false;

  bool isValid(const int8_t* setup) const;
  int countCompletions(int8_t* setup, int row, int limit) const;
  bool solveRandom(int8_t* setup, int row);
  void shuffle(int8_t* values, int count) const;
  void chooseDifficulty();
  void generateNextPuzzle();
  void newPuzzle();
  void completePuzzle();

 public:
  explicit NQueensActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NQueens", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
