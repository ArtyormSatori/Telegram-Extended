#include "telegram_media/capability_registry.hpp"

#include <sstream>
#include <utility>

namespace telegram_media {
namespace {
std::vector<std::string> split_words(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> words;
    std::string word;
    while (input >> word) words.push_back(std::move(word));
    return words;
}
std::string trim_line(std::string line) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = line.find_last_not_of(" \t\r");
    return line.substr(first, last - first + 1U);
}
bool contains_format(const std::set<std::string>& formats, const std::string& name) {
    if (formats.contains(name)) return true;
    const auto comma = name.find(',');
    return comma != std::string::npos && formats.contains(name.substr(0, comma));
}
} // namespace

bool CodecCapabilities::supports_video_encoder(const std::string& codec) const {
    return video_encoders.contains(codec);
}
bool CodecCapabilities::supports_audio_encoder(const std::string& codec) const {
    return audio_encoders.contains(codec);
}
bool CodecCapabilities::supports_muxer(const std::string& format) const {
    return contains_format(muxers, format);
}

CodecCapabilities CodecCapabilities::parse(const std::string& encoders_output,
                                           const std::string& formats_output) {
    CodecCapabilities result;
    std::istringstream encoder_lines(encoders_output);
    std::string line;
    while (std::getline(encoder_lines, line)) {
        line = trim_line(std::move(line));
        if (line.size() < 8U || line[0] == '#') continue;
        const auto words = split_words(line);
        if (words.size() < 2U || words[0].size() < 6U || words[1] == "=") continue;
        const auto& flags = words[0];
        const auto& codec = words[1];
        if (flags[0] == 'V') result.video_encoders.insert(codec);
        else if (flags[0] == 'A') result.audio_encoders.insert(codec);
        else if (flags[0] == 'S') result.subtitle_encoders.insert(codec);
    }

    std::istringstream format_lines(formats_output);
    while (std::getline(format_lines, line)) {
        line = trim_line(std::move(line));
        if (line.size() < 2U || line[0] == '#') continue;
        const auto words = split_words(line);
        if (words.size() < 2U || words[0].empty()) continue;
        const auto& flags = words[0];
        const auto& names = words[1];
        std::size_t start = 0;
        while (start <= names.size()) {
            const auto end = names.find(',', start);
            const auto token = names.substr(start, end == std::string::npos
                ? std::string::npos : end - start);
            if (!token.empty()) {
                if (flags.find('D') != std::string::npos) result.demuxers.insert(token);
                if (flags.find('E') != std::string::npos) result.muxers.insert(token);
            }
            if (end == std::string::npos) break;
            start = end + 1U;
        }
    }
    return result;
}

FFmpegCapabilityProbe::FFmpegCapabilityProbe(std::string executable)
    : executable_(std::move(executable)) {}

CapabilityResult FFmpegCapabilityProbe::inspect(const IProcessRunner& runner,
                                                std::chrono::milliseconds timeout) const {
    CapabilityResult result;
    if (executable_.empty()) {
        result.errors.emplace_back("FFmpeg executable is required.");
        return result;
    }
    const auto encoders = runner.run({executable_, "-hide_banner", "-encoders"}, timeout);
    if (!encoders.launch_error.empty() || encoders.timed_out || encoders.exit_code != 0 ||
        encoders.output_limit_exceeded) {
        result.errors.emplace_back(!encoders.launch_error.empty() ? encoders.launch_error :
            encoders.timed_out ? "FFmpeg encoder discovery timed out." :
            encoders.output_limit_exceeded ? "FFmpeg encoder output exceeded the limit." :
            encoders.standard_error.empty() ? "FFmpeg encoder discovery failed." : encoders.standard_error);
        return result;
    }
    const auto formats = runner.run({executable_, "-hide_banner", "-formats"}, timeout);
    if (!formats.launch_error.empty() || formats.timed_out || formats.exit_code != 0 ||
        formats.output_limit_exceeded) {
        result.errors.emplace_back(!formats.launch_error.empty() ? formats.launch_error :
            formats.timed_out ? "FFmpeg format discovery timed out." :
            formats.output_limit_exceeded ? "FFmpeg format output exceeded the limit." :
            formats.standard_error.empty() ? "FFmpeg format discovery failed." : formats.standard_error);
        return result;
    }
    result.capabilities = CodecCapabilities::parse(
        encoders.standard_output + "\n" + encoders.standard_error,
        formats.standard_output + "\n" + formats.standard_error);
    if (result.capabilities.video_encoders.empty() && result.capabilities.audio_encoders.empty()) {
        result.errors.emplace_back("FFmpeg did not report any usable encoders.");
        return result;
    }
    result.ok = true;
    return result;
}
} // namespace telegram_media
