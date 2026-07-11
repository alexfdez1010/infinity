#include "GameScores.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

namespace {
// v2: version, best2048, mazeBest[3] (maze game removed)
// v3: version, best2048
// v4: version, best2048, bestApagon
// v5: version, best2048, bestApagon, bestSlide, bestPeg
// v6: ...v5 fields, bestMaze[3], bestBlocks[3]
constexpr uint8_t SCORES_FILE_VERSION = 6;
constexpr char SCORES_FILE[] = "/.crosspoint/game_scores.bin";
}  // namespace

GameScores GameScores::instance;

bool GameScores::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  FsFile file;
  if (!Storage.openFileForWrite("GSC", SCORES_FILE, file)) {
    LOG_ERR("GSC", "Failed to open game_scores.bin for write");
    return false;
  }
  const uint8_t version = SCORES_FILE_VERSION;
  serialization::writePod(file, version);
  serialization::writePod(file, best2048);
  serialization::writePod(file, bestApagon);
  serialization::writePod(file, bestSlide);
  serialization::writePod(file, bestPeg);
  for (int i = 0; i < 3; i++) serialization::writePod(file, bestMaze[i]);
  for (int i = 0; i < 3; i++) serialization::writePod(file, bestBlocks[i]);
  file.close();
  return true;
}

bool GameScores::loadFromFile() {
  FsFile file;
  if (!Storage.openFileForRead("GSC", SCORES_FILE, file)) {
    return false;  // first boot — file doesn't exist yet
  }
  uint8_t version;
  serialization::readPod(file, version);
  // v2..v6 all begin with best2048; v4 appends bestApagon, v5 appends bestSlide
  // and bestPeg, v6 appends bestMaze[3] and bestBlocks[3]. The trailing v2 maze
  // data is ignored, and older files leave the newer fields at their defaults.
  if (version < 2 || version > SCORES_FILE_VERSION) {
    LOG_ERR("GSC", "Unknown game_scores.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, best2048);
  if (version >= 4) {
    serialization::readPod(file, bestApagon);
  }
  if (version >= 5) {
    serialization::readPod(file, bestSlide);
    serialization::readPod(file, bestPeg);
  }
  if (version >= 6) {
    for (int i = 0; i < 3; i++) serialization::readPod(file, bestMaze[i]);
    for (int i = 0; i < 3; i++) serialization::readPod(file, bestBlocks[i]);
  }
  file.close();
  return true;
}
