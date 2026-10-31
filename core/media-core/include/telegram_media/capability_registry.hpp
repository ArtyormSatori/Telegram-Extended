#pragma once

#include "telegram_media/process_runner.hpp"

#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace telegram_media {

struct CodecCapabilities {
    std::set<std::string> video_encoders;
    std::set<std::string> audio_encoders;
    std::set<std::string> subtitle_encoders;
    std::set<std::string> muxers;
    std::set<std::string> demuxers;

    [[nodiscard]] bool supports_video_encoder(const std::string& codec) const;
    [[nodiscard]] bool supports_audio_encoder(const std::string& codec) const;
    [[nodiscard]] bool supports_muxer(const std::string& format) const;
    [[nodiscard]] static CodecCapabilities parse(const std::string& encoders_output,
                                                  const std::string& formats_output);
};

struct CapabilityResult {
    bool ok{false};
    CodecCapabilities capabilities;
    std::vector<std::string> errors;
};

class FFmpegCapabilityProbe {
public:
    explicit FFmpegCapabilityProbe(std::string executable = "ffmpeg");
    [[nodiscard]] CapabilityResult inspect(
        const IProcessRunner& runner,
        std::chrono::milliseconds timeout = std::chrono::seconds(20)) const;
private:
    std::string executable_;
};

} // namespace telegram_media
