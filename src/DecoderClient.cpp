#include <boost/asio.hpp>
#include <fmt/core.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <iostream>

#include "DecoderClient.hpp"

namespace mik {

DecoderClient::DecoderClient() : ioContext_(), socket_(ioContext_) {
  if (spdlog::get("AlsaLogger")) {
    // The logger exists already
    logger_ = spdlog::get("AlsaLogger");
    logger_->set_level(spdlog::level::debug);
  } else {
    try {
      logger_ = spdlog::basic_logger_mt("AlsaLogger", "logs/client.log", true);
      logger_->set_level(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex& e) {
      std::cerr << "Log init failed with spdlog_ex: " << e.what() << std::endl;
    }
  }

  logger_->info("--------------------------------------------");
  logger_->info("DecoderClient created.");
  logger_->info("--------------------------------------------");
  logger_->set_level(spdlog::level::level_enum::debug);
  logger_->debug("Debug-level logging enabled");
  logger_->flush();
}

void DecoderClient::connect(std::string_view host, std::string_view port) {

  boost::asio::ip::tcp::resolver res(ioContext_);
  boost::asio::ip::tcp::resolver::results_type endpoints;

  try {
    endpoints = res.resolve(host, port);

  } catch (const boost::system::system_error& e) {
    logger_->error("Error {}", e.what());
    return;
  }

  logger_->info("Retrieved endpoints");
  logger_->info("Attempting to connect to {}:{}...", host, port);
  logger_->flush(); // Flushing since connect() blocks

  try {
    boost::asio::connect(socket_, endpoints);
    logger_->info("Connected.");

  } catch (const boost::system::system_error& e) {
    logger_->info("Caught exception on connect(): {}", e.what());
    return;
  }
}

size_t DecoderClient::sendAudioToServer(const std::vector<uint8_t>& buffer) {
  logger_->debug("Writing to the socket...");

  try {
    const boost::asio::const_buffer boostBuffer(buffer.data(), buffer.size());
    const auto bytesWritten = boost::asio::write(socket_, boostBuffer);
    logger_->debug("Wrote {} bytes of audio to the socket", bytesWritten);
    return bytesWritten;
  } catch (const boost::system::system_error& e) {
    logger_->error("Couldn't write to socket: {}", e.what());
    return 0;
  }
}

std::string DecoderClient::getResultFromServer() {
  // Read in reply
  boost::asio::streambuf streamBuf;
  size_t bytesRead = 0;
  try {
    bytesRead = boost::asio::read_until(socket_, streamBuf, "\n");
  } catch (const boost::system::system_error& e) {
    logger_->error("Got exception while reading from socket: {}", e.what());
    return "";
  }
  logger_->debug("Read in {} bytes of results", bytesRead);

  const auto bufIter = streamBuf.data();
  // Create the string based off of the streamBuf range
  const std::string result(boost::asio::buffers_begin(bufIter),
                           boost::asio::buffers_begin(bufIter) + static_cast<long>(bytesRead));
  if (result.empty()) {
    logger_->error("The result from the decoder was empty");
  } else {
    logger_->debug("Result:{}", result);
  }
  return result;
}

std::streampos findSizeOfFileStream(std::istream& str) {
  const auto originalPos = str.tellg();    // Find current pos
  str.seekg(0, str.end);                   // Go back to start
  const std::streampos size = str.tellg(); // Find size
  str.seekg(originalPos, str.beg);         // Reset pos back to where we were
  return size;
}

std::vector<uint8_t> Utils::readInAudiofile(std::string_view filename) {

  auto logger = spdlog::get("DecoderClientLogger");

  std::fstream inputStream(std::string(filename), inputStream.in | std::fstream::binary);
  std::istream_iterator<uint8_t> inputIter(inputStream);

  if (!inputStream.is_open()) {
    logger->error("Could not open inputfile for reading\n");
    return {};
  }

  const auto inputSize = findSizeOfFileStream(inputStream);
  logger->debug("{} is {} bytes", filename, inputSize);

  // Read in bufferSize amount of bytes from the audio file into the buffer
  std::vector<uint8_t> audioBuffer;
  audioBuffer.reserve(static_cast<std::vector<uint8_t>::size_type>(inputSize));
  logger->debug("audioBuffer has {} bytes before reading in file", audioBuffer.size());

  // Read in entire file
  std::copy(inputIter, std::istream_iterator<uint8_t>(), std::back_inserter(audioBuffer));
  // inputStream.read(audioBuffer.data(), inputSize);

  const auto bytesRead = inputStream.gcount();
  logger->debug("Read in {} out of {} bytes from audio file", bytesRead, inputSize);
  logger->debug("audioBuffer has {} bytes after reading in file", audioBuffer.size());

  return audioBuffer;
}

} // namespace mik
