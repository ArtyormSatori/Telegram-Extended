#pragma once

#include "telegram_media/capability_registry.hpp"
#include "telegram_media/ffprobe_adapter.hpp"
#include "telegram_media/ffmpeg_command_builder.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace telegram_media {

struct ConversionExecutionRequest {
    std::string source_path;
    std::string output_path;
    MediaInfo source_info;
    ConversionRequest request;
    ConversionPlan plan;
    std::chrono::milliseconds timeout{std::chrono::minutes(10)};
};

struct ConversionExecutionResult {
    bool ok{false};
    std::string output_path;
    ProcessResult process;
    ProbeResult output_info;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class FFmpegConversionExecutor {
public:
    FFmpegConversionExecutor(std::string ffmpeg_executable = "ffmpeg",
                             std::string ffprobe_executable = "ffprobe");
    [[nodiscard]] ConversionExecutionResult execute(
        const ConversionExecutionRequest& request,
        const CodecCapabilities& capabilities,
        const IProcessRunner& runner) const;
private:
    std::string ffmpeg_executable_;
    FFprobeAdapter ffprobe_;
};

} // namespace telegram_media
