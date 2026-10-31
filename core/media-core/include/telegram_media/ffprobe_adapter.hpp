#pragma once
#include "telegram_media/media_types.hpp"
#include "telegram_media/process_runner.hpp"
#include <chrono>
#include <string>
#include <vector>
namespace telegram_media {
struct ProbeResult {
    bool ok{false};
    MediaInfo media;
    std::vector<std::string> errors;
};
class FFprobeAdapter {
public:
    explicit FFprobeAdapter(std::string executable = "ffprobe");
    [[nodiscard]] std::vector<std::string> build_arguments(const std::string& local_path) const;
    [[nodiscard]] ProbeResult probe(const std::string& local_path,
        const IProcessRunner& runner,
        std::chrono::milliseconds timeout = std::chrono::seconds(30)) const;
    [[nodiscard]] static ProbeResult parse_flat_output(const std::string& output);
private:
    std::string executable_;
};
} // namespace telegram_media
