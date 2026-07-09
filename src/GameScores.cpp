#include "GameScores.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

namespace {
// v2: version, best2048, mazeBest[3] (maze game removed)
// v3: version, best2048
// v4: version, best2048, bestApagon
constexpr uint8_t SCORES_FILE_VERSION = 4;
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
  // v2, v3 and v4 all begin with best2048; v4 appends bestApagon. The trailing
  // v2 maze data is ignored, and pre-v4 files leave bestApagon at its default.
  if (version != 2 && version != 3 && version != SCORES_FILE_VERSION) {
    LOG_ERR("GSC", "Unknown game_scores.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, best2048);
  if (version >= 4) {
    serialization::readPod(file, bestApagon);
  }
  file.close();
  return true;
}
