
#include "AlsaInterface.hpp"

namespace mik {


inline snd_pcm_t* AlsaInterface::getSoundDeviceHandle(std::string_view captureDevice) {
    snd_pcm_t* pcmHandle = nullptr;
    const int err = snd_pcm_open(&pcmHandle, captureDevice.data(), SND_PCM_STREAM_CAPTURE, 0x0);
    if (err != 0) {
        logger_->error("Couldn't get capture handle, error:{}", std::strerror(err));
        return nullptr;
    }
    return pcmHandle;
}


AlsaInterface::AlsaInterface(std::string_view pcmDesc) {

    logger_ = std::make_unique<spdlog::logger>("AlsaCaptureLogger", logFileSink);
    logger_->info("Initialized logger");

    // Reference: https://www.linuxjournal.com/article/6735
    pcmHandle_.reset(getSoundDeviceHandle(pcmDesc));
    if (!pcmHandle_) {
        logger_->error("Could not get handle for the capture device: {}.", pcmDesc);
        return;
    }

    logger_->info("Allocating memory for parameters...");
    {
        snd_pcm_hw_params_t* params = nullptr;
        snd_pcm_hw_params_alloca(&params);
        params_ = params; // Grab the pointer we got from the ALSA macro
    }

    logger_->info("Setting sound card parameters...");
    snd_pcm_hw_params_any(pcmHandle_.get(), params_);
    snd_pcm_hw_params_set_format(pcmHandle_.get(), params_, config_.format);
    snd_pcm_hw_params_set_channels(pcmHandle_.get(), params_, static_cast<unsigned int>(config_.channelConfig));
    snd_pcm_hw_params_set_access(pcmHandle_.get(), params_, config_.accessType);

    // Set sampling rate
    int dir = 0;
    unsigned int val = config_.samplingRate_bps;
    logger_->info("Attempting to get a sampling rate of {} bits per second", val);
    snd_pcm_hw_params_set_rate_near(pcmHandle_.get(), params_, &val, &dir);
    logger_->info("Got a sampling rate of {} bits per second", val);

    // Set period size to X frames
    logger_->info("Attempting to set period size to {} frames", config_.frames);
    snd_pcm_hw_params_set_period_size_near(pcmHandle_.get(), params_, &(config_.frames), &dir);
    logger_->info("Period size was set to {} frames", config_.frames);

    // Write parameters to the driver
    int status = snd_pcm_hw_params(pcmHandle_.get(), params_);
    if (status != 0) {
        logger_->error("Could not write parameters to the sound driver!");
        logger_->error("snd_pcm_hw_params() errno:{}", std::strerror(errno));
        return;
    }

    // Create a buffer to hold a period
    snd_pcm_hw_params_get_period_size(params_, &(config_.frames), &dir);
    logger_->info("Retrieved period size of {} frames", config_.frames);
    
    // Figure out how long a period is
    snd_pcm_hw_params_get_period_time(params_, &(config_.recordingPeriod_us), &dir);
    if (config_.recordingPeriod_us == 0) {
        logger_->error("Can't divide by a recording period of 0ms!");
        std::exit(1);
    }
    logger_->info("Retrieved recording period of {} ms", config_.recordingPeriod_us);

    size_t bufferSize = config_.frames * 4; // 2 bytes/sample, 2 channel
    logger_->info("Buffer size is {} bytes", bufferSize);
    buffer_.reset(static_cast<char*>(std::malloc(bufferSize)));
    bufferSize_ = bufferSize;

}

}
