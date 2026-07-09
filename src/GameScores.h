#pragma once
#include <cstdint>

// Persists high scores for games to /.crosspoint/game_scores.bin
// Loaded once at boot alongside ReadingStats. Each game saves on improvement.
class GameScores {
  static GameScores instance;

 public:
  uint32_t best2048 = 0;    // 2048 best score
  uint32_t bestApagon = 0;  // Apagón fewest moves to solve (0 = no record yet)
  uint32_t bestSlide = 0;   // Sliding puzzle fewest moves to solve (0 = no record yet)
  uint32_t bestPeg = 0;     // Senku fewest pegs left (1 = perfect, 0 = no record yet)

  static GameScores& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
};

#define GAME_SCORES GameScores::getInstance()
