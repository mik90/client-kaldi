#include <docopt/docopt.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>

#include "AlsaInterface.hpp"
#include "Utils.hpp"

static constexpr auto Usage =
    R"(AlsaInterface.

    Usage:
          AlsaInterface capture --file <output_filename>
          AlsaInterface playback --file <input_audio_file>
          AlsaInterface (-h | --help)
          AlsaInterface (-v | --version)

    Options:
          -h --help             Show this screen.
          -v --version          Show the version.
)";

int main(int argc, char** argv) {

  auto args = docopt::docopt(Usage, {std::next(argv), std::next(argv, argc)},
                             true,                 // show help if requested
                             "AlsaInterface 0.1"); // version string
  fmt::print("--------\ndocopt output:\n");
  for (auto const& arg : args) {
    std::cout << arg.first << ": " << arg.second << std::endl;
  }
  fmt::print("--------\n");

  const auto capture = args[std::string("capture")];
  const auto playback = args[std::string("playback")];

  const auto outputFilename = args[std::string("<output_filename>")];
  const auto inputAudioFile = args[std::string("<input_audio_file>")];

  mik::Utils::createLogger();
  // Use the default config
  mik::AlsaConfig config;
  config.samplingFreq_Hz = 8000;
  mik::AlsaInterface alsa(config);

  if (capture && outputFilename) {
    std::fstream outputStream(outputFilename.asString(), outputStream.trunc | outputStream.out);

    if (!outputStream.is_open()) {
      SPDLOG_ERROR("Could not open {} for creating/writing", outputFilename.asString());
      std::exit(1);
    }
    alsa.captureAudioFixedSize(outputStream, 15);
  } else if (playback && inputAudioFile) {
    std::fstream inputStream(inputAudioFile.asString(), inputStream.in);
    if (!inputStream.is_open()) {
      SPDLOG_ERROR("Could not open {} for reading", inputAudioFile.asString());
      std::exit(1);
    }
    alsa.playbackAudioFixedSize(inputStream, 15);
  }

  return 0;
}
