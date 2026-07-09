#pragma once
#include <cstdint>

// Persists high scores for games to /.crosspoint/game_scores.bin
// Loaded once at boot alongside ReadingStats. Each game saves on improvement.
class GameScores {
  static GameScores instance;

 public:
  uint32_t best2048 = 0; // 2048 best score

  static GameScores& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
};

#define GAME_SCORES GameScores::getInstance()
