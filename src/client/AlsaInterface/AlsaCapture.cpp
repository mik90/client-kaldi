#include <chrono>
#include <condition_variable>
#include <thread>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "AlsaInterface.hpp"
#include "Utils.hpp"

namespace mik {

// --------------------------------------------------------------------------------------
// AlsaInterface::startRecording
// --------------------------------------------------------------------------------------
void AlsaInterface::startRecording() {
  // Create the thread objec which will start off the recording
  shouldRecord_ = true;
  recordingThread_ = std::thread(&AlsaInterface::record, this);
  SPDLOG_INFO("Recording started.");
}

// --------------------------------------------------------------------------------------
// AlsaInterface::stopRecording
// --------------------------------------------------------------------------------------
void AlsaInterface::stopRecording() {
  shouldRecord_ = false;
  if (recordingThread_.joinable()) {
    recordingThread_.join();
  }
  if (pcmHandle_) {
    snd_pcm_drop(pcmHandle_.get());
  }
  SPDLOG_INFO("Recording stopped.");
}

// --------------------------------------------------------------------------------------
// AlsaInterface::record
// --------------------------------------------------------------------------------------
void AlsaInterface::record() {
  SPDLOG_DEBUG("record(): start");

  // Allocate a chunk of data for the buffer and wrap it in a unique_ptr
  auto cBuffer = std::make_unique<uint8_t[]>(config_.periodSizeBytes);

  while (shouldRecord_) {

    // Read data from the sound card into audioChunk
    const auto status = snd_pcm_readi(pcmHandle_.get(), cBuffer.get(), config_.frames);
    if (status == -EPIPE) {
      // Overran the buffer
      SPDLOG_WARN("record(): Overran buffer, received EPIPE. Will continue");
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    } else if (status < 0) {
      SPDLOG_ERROR("record(): Error reading from pcm. errno:{}",
                   std::strerror(static_cast<int>(status)));
      return;
    } else if (status != static_cast<int>(config_.frames)) {
      SPDLOG_WARN("record(): Should've read {} frames, only read {}.", config_.frames, status);
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    }

    {
      // Access the container that holds all the auido data, add this chunk of audio to it
      std::scoped_lock<std::mutex> lock(audioChunkMutex_);

      // Pointer arithmetic, messy but i want to move the data from the C-style array into audioData
      std::move(cBuffer.get(), cBuffer.get() + config_.periodSizeBytes,
                std::back_inserter(audioData_));
    }
  }

  SPDLOG_DEBUG("record(): end");
}

// --------------------------------------------------------------------------------------
// AlsaInterface::captureAudioUntilUserExit
// --------------------------------------------------------------------------------------
std::vector<char> AlsaInterface::captureAudioUntilUserExit() {
  SPDLOG_INFO("Starting capture until user exits...");

  // Capture audio until user presses a key
  if (!this->isConfiguredForCapture()) {
    SPDLOG_DEBUG("Interface was not configured for audio capture! Re-configuring...");
    config_.streamConfig = StreamConfig::CAPTURE;
    // Re-configure the interface with the new config
    this->configureInterface();
  }

  const auto start = std::chrono::steady_clock::now();
  fmt::print("Starting recording...\n");
  this->startRecording();

  fmt::print("Press <ENTER> to stop recording\n");
  [[maybe_unused]] std::string input;
  std::getline(std::cin, input);

  this->stopRecording();

  const auto end = std::chrono::steady_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  const auto secondsAsFraction = static_cast<double>(duration.count()) / 1000.0;
  const auto infoMsg =
      fmt::format("Recording stopped, received {} seconds of audio totalling {:L} bytes",
                  secondsAsFraction, audioData_.size());
  SPDLOG_DEBUG(infoMsg);
  fmt::print("{}\n", infoMsg);

  return audioData_;
}

// --------------------------------------------------------------------------------------
// AlsaInterface::captureAudioFixedSizeMs
// --------------------------------------------------------------------------------------
std::vector<char> AlsaInterface::captureAudioFixedSizeMs(unsigned int milliseconds) {
  SPDLOG_INFO("Starting capture...");

  const auto recordingDuration_us = Utils::millisecondsToMicroseconds(milliseconds);
  if (!this->isConfiguredForCapture()) {
    SPDLOG_INFO("Interface was not configured for audio capture! Re-configuring...");
    config_.streamConfig = StreamConfig::CAPTURE;
    // Re-configure the interface with the new config
    this->configureInterface();
  }

  SPDLOG_DEBUG("Entire recording duration is {} microseconds, each period is {} microseconds",
               recordingDuration_us, config_.periodDuration_us);

  auto loopsLeft = static_cast<unsigned int>(config_.calculateRecordingLoops(recordingDuration_us));
  SPDLOG_INFO("Running {} recording iterations", loopsLeft);

  std::vector<char> outputBuffer;
  outputBuffer.resize(config_.periodSizeBytes * loopsLeft);
  size_t bytesRead = 0;

  auto startTime = std::chrono::steady_clock::now();
  while (loopsLeft > 0) {
    --loopsLeft;
    auto status = snd_pcm_readi(pcmHandle_.get(), outputBuffer.data() + bytesRead, config_.frames);
    bytesRead += config_.periodSizeBytes;

    if (status == -EPIPE) {
      // Overran the buffer
      SPDLOG_WARN("Overran buffer, received EPIPE. Will continue");
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    } else if (status < 0) {
      SPDLOG_ERROR("Error reading from pcm. errno:{}", std::strerror(static_cast<int>(status)));
      return {};
    } else if (status != static_cast<int>(config_.frames)) {
      SPDLOG_WARN("Should've read {} frames, only read {}.", config_.frames, status);
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    }
  }

  auto endTime = std::chrono::steady_clock::now();
  const auto actualDuration =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  SPDLOG_INFO("Capture was configured to take {} milliseconds, it actually took {} ms",
              recordingDuration_us / 1000, actualDuration);

  snd_pcm_drop(pcmHandle_.get());
  SPDLOG_INFO("Finished audio capture");
  return outputBuffer;
}

// --------------------------------------------------------------------------------------
// AlsaInterface::captureAudioFixedSizeMs
// --------------------------------------------------------------------------------------
void AlsaInterface::captureAudioFixedSizeMs(std::ostream& outputStream, unsigned int milliseconds) {
  SPDLOG_INFO("Starting capture...");

  const auto recordingDuration_us = Utils::millisecondsToMicroseconds(milliseconds);
  if (!this->isConfiguredForCapture()) {
    SPDLOG_DEBUG("Interface was not configured for audio capture! Re-configuring...");
    config_.streamConfig = StreamConfig::CAPTURE;
    // Re-configure the interface with the new config
    this->configureInterface();
  }

  SPDLOG_DEBUG("Entire recording duration is {} microseconds, each period is {} microseconds",
               recordingDuration_us, config_.periodDuration_us);

  int loopsLeft = config_.calculateRecordingLoops(recordingDuration_us);

  auto startTime = std::chrono::steady_clock::now();

  // The output stream wants this to be a char instead of uint_8, just make it a char
  auto cBuffer = std::make_unique<char[]>(config_.periodSizeBytes);

  SPDLOG_INFO("Running {} recording iterations", loopsLeft);
  SPDLOG_INFO("PCM State: {}", snd_pcm_state_name(snd_pcm_state(pcmHandle_.get())));

  while (loopsLeft > 0) {
    --loopsLeft;
    auto status = snd_pcm_readi(pcmHandle_.get(), cBuffer.get(), config_.frames);
    if (status == -EPIPE) {
      // Overran the buffer
      SPDLOG_WARN("Overran buffer, received EPIPE. Will continue");
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    } else if (status < 0) {
      SPDLOG_ERROR("Error reading from pcm. errno:{}", std::strerror(static_cast<int>(status)));
      return;
    } else if (status != static_cast<int>(config_.frames)) {
      SPDLOG_WARN("Should've read {} frames, only read {}.", config_.frames, status);
      snd_pcm_prepare(pcmHandle_.get());
      continue;
    }

    outputStream.write(cBuffer.get(), static_cast<std::streamsize>(config_.periodSizeBytes));
  }

  auto endTime = std::chrono::steady_clock::now();
  const auto actualDuration =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  SPDLOG_INFO("Capture was configured to take {} milliseconds, it actually took {} ms",
              recordingDuration_us / 1000, actualDuration);

  snd_pcm_drop(pcmHandle_.get());
  SPDLOG_INFO("Finished audio capture");
  return;
}

} // namespace mik
