#pragma once

#include "telegram_media/capability_registry.hpp"
#include "telegram_media/media_types.hpp"

#include <string>
#include <vector>

namespace telegram_media {

struct FFmpegCommandResult {
    bool ok{false};
    std::vector<std::string> arguments;
    std::vector<std::string> errors;
};

class FFmpegCommandBuilder {
public:
    [[nodiscard]] static FFmpegCommandResult build(
        const std::string& executable,
        const std::string& source_path,
        const std::string& output_path,
        const MediaInfo& source_info,
        const ConversionRequest& request,
        const ConversionPlan& plan,
        const CodecCapabilities& capabilities);
};

} // namespace telegram_media
