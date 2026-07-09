#pragma once

#include <functional>
#include <string>

class OtaUpdater {
  bool updateAvailable = false;
  std::string latestVersion;
  std::string otaUrl;
  size_t otaSize = 0;
  size_t processedSize = 0;
  size_t totalSize = 0;
  bool render = false;
  // esp_https_ota_handle_t is typedef void* — use void* to avoid pulling in
  // esp_https_ota.h from the header (causes lwip header-ordering conflicts).
  void* otaHandle = nullptr;
  bool cancelRequested = false;

 public:
  enum OtaUpdaterError {
    OK = 0,
    NO_UPDATE,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    UPDATE_OLDER_ERROR,
    INTERNAL_UPDATE_ERROR,
    OOM_ERROR,
    UPDATE_CANCELLED,
    UPDATE_IN_PROGRESS,
  };

  size_t getOtaSize() const { return otaSize; }
  size_t getProcessedSize() const { return processedSize; }
  size_t getTotalSize() const { return totalSize; }
  bool getRender() const { return render; }
  bool isUpdateInProgress() const { return otaHandle != nullptr; }
  void clearRender() { render = false; }

  OtaUpdater() = default;
  bool isUpdateNewer() const;
  const std::string& getLatestVersion() const;
  OtaUpdaterError checkForUpdate();
  OtaUpdaterError installUpdate();  // synchronous convenience wrapper (legacy)

  // Non-blocking OTA: call beginInstallUpdate() once, then performInstallUpdateStep()
  // each render tick. Returns UPDATE_IN_PROGRESS while ongoing, OK on success.
  OtaUpdaterError beginInstallUpdate();
  OtaUpdaterError performInstallUpdateStep();
  void cancelUpdate();
  void cleanupUpdate();
};
