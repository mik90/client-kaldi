#include <boost/asio.hpp>
#include <fmt/core.h>
#include <fmt/locale.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <iostream>

#include "DecoderClient.hpp"
#include "Utils.hpp"

namespace mik {

DecoderClient::DecoderClient() : ioContext_(), socket_(ioContext_) {
  SPDLOG_INFO("--------------------------------------------");
  SPDLOG_INFO("DecoderClient created.");
  SPDLOG_INFO("--------------------------------------------");
}

void DecoderClient::connect(std::string_view host, std::string_view port) {

  boost::asio::ip::tcp::resolver res(ioContext_);
  boost::asio::ip::tcp::resolver::results_type endpoints;

  try {
    endpoints = res.resolve(host, port);

  } catch (const boost::system::system_error& e) {
    SPDLOG_ERROR("Error {}", e.what());
    return;
  }

  SPDLOG_INFO("Retrieved endpoints");
  SPDLOG_INFO("Attempting to connect to {}:{}...", host, port);

  try {
    boost::asio::connect(socket_, endpoints);
    SPDLOG_INFO("Connected.");

  } catch (const boost::system::system_error& e) {
    SPDLOG_INFO("Caught exception on connect(): {}", e.what());
    return;
  }
}

size_t DecoderClient::sendAudioToServer(const std::vector<char>& buffer) {
  SPDLOG_DEBUG("Writing to the socket...");

  try {
    const boost::asio::const_buffer boostBuffer(buffer.data(), buffer.size());
    SPDLOG_DEBUG(fmt::format(std::locale("en_US.UTF-8"), "Boost buffer is {:L} bytes large",
                             boostBuffer.size()));

    const auto bytesWritten = boost::asio::write(socket_, boostBuffer);
    SPDLOG_DEBUG(fmt::format(std::locale("en_US.UTF-8"), "Wrote {:L} bytes of audio to the socket",
                             bytesWritten));

    return bytesWritten;
  } catch (const boost::system::system_error& e) {
    SPDLOG_ERROR("Couldn't write to socket: {}", e.what());
    return 0;
  }
}

std::string DecoderClient::getResultFromServer() {
  // Read in reply
  boost::asio::streambuf streamBuf;
  size_t bytesRead = 0;
  boost::system::error_code error;
  while (!error) {
    bytesRead += boost::asio::read(socket_, streamBuf, error);
  }
  if (error == boost::asio::error::eof) {
    SPDLOG_DEBUG("Read until EOF");
  } else if (error) {
    SPDLOG_ERROR("Got exception while reading from socket: {}", error.message());
    return std::string();
  }

  SPDLOG_DEBUG("Read in {} bytes of results", bytesRead);

  const auto bufIter = streamBuf.data();
  // Create the string based off of the streamBuf range
  const std::string result(boost::asio::buffers_begin(bufIter),
                           boost::asio::buffers_begin(bufIter) + static_cast<long>(bytesRead));
  if (result.empty()) {
    SPDLOG_ERROR("The result from the decoder was empty");
  } else {
    SPDLOG_DEBUG("Result:{}", result);
  }
  return result;
}

} // namespace mik
