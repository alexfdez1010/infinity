#include "GameScores.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

namespace {
// v2: version, best2048, mazeBest[3] (maze game removed)
// v3: version, best2048
constexpr uint8_t SCORES_FILE_VERSION = 3;
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
  // v2 and v3 both begin with best2048; the trailing v2 maze data is ignored.
  if (version != 2 && version != SCORES_FILE_VERSION) {
    LOG_ERR("GSC", "Unknown game_scores.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, best2048);
  file.close();
  return true;
}
