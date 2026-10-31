#include "telegram_media/ffmpeg_conversion_executor.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <utility>

namespace telegram_media {
namespace {
std::atomic_uint64_t temp_sequence{0};

std::size_t count_kind(const MediaInfo& info, MediaKind kind) {
    return static_cast<std::size_t>(std::count_if(info.streams.begin(), info.streams.end(),
        [kind](const StreamInfo& stream) { return stream.kind == kind; }));
}
bool has_hdr(const MediaInfo& info) {
    return std::any_of(info.streams.begin(), info.streams.end(),
        [](const StreamInfo& stream) { return stream.kind == MediaKind::Video && stream.hdr; });
}
bool format_matches(const std::string& reported, const std::string& requested) {
    std::size_t start = 0;
    while (start <= reported.size()) {
        const auto end = reported.find(',', start);
        const auto item = reported.substr(start, end == std::string::npos
            ? std::string::npos : end - start);
        if (item == requested) return true;
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return false;
}
std::filesystem::path make_temp_path(const std::filesystem::path& output) {
    const auto parent = output.parent_path().empty()
        ? std::filesystem::current_path() : output.parent_path();
    const auto stem = output.stem().string();
    const auto extension = output.extension().string();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto sequence = temp_sequence.fetch_add(1U);
        const auto name = stem + ".te-partial-" + std::to_string(tick) + "-" +
                          std::to_string(sequence) + extension;
        const auto candidate = parent / name;
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) return candidate;
    }
    return {};
}
void remove_if_present(const std::filesystem::path& path,
                       std::vector<std::string>& warnings) {
    std::error_code error;
    (void)std::filesystem::remove(path, error);
    if (error) warnings.emplace_back("Could not remove temporary output: " + error.message());
}
} // namespace

FFmpegConversionExecutor::FFmpegConversionExecutor(std::string ffmpeg_executable,
                                                   std::string ffprobe_executable)
    : ffmpeg_executable_(std::move(ffmpeg_executable)), ffprobe_(std::move(ffprobe_executable)) {}

ConversionExecutionResult FFmpegConversionExecutor::execute(
    const ConversionExecutionRequest& request,
    const CodecCapabilities& capabilities,
    const IProcessRunner& runner) const {
    ConversionExecutionResult result;
    if (request.timeout <= std::chrono::milliseconds::zero()) {
        result.errors.emplace_back("Conversion timeout must be positive.");
        return result;
    }
    const std::filesystem::path final_path(request.output_path);
    const auto temporary_path = make_temp_path(final_path);
    if (temporary_path.empty()) {
        result.errors.emplace_back("Could not allocate a unique temporary output path.");
        return result;
    }
    const auto command = FFmpegCommandBuilder::build(ffmpeg_executable_, request.source_path,
        temporary_path.string(), request.source_info, request.request, request.plan, capabilities);
    if (!command.ok) {
        result.errors = command.errors;
        return result;
    }

    result.process = runner.run(command.arguments, request.timeout);
    if (!result.process.launch_error.empty()) result.errors.emplace_back(result.process.launch_error);
    else if (result.process.timed_out) result.errors.emplace_back("FFmpeg conversion timed out.");
    else if (result.process.output_limit_exceeded) result.errors.emplace_back("FFmpeg process output exceeded the configured limit.");
    else if (result.process.exit_code != 0) result.errors.emplace_back(result.process.standard_error.empty()
        ? "FFmpeg conversion failed." : result.process.standard_error);
    if (!result.errors.empty()) {
        remove_if_present(temporary_path, result.warnings);
        return result;
    }

    std::error_code file_error;
    if (!std::filesystem::is_regular_file(temporary_path, file_error) || file_error ||
        std::filesystem::file_size(temporary_path, file_error) == 0U || file_error) {
        result.errors.emplace_back("FFmpeg did not produce a non-empty regular output file.");
        remove_if_present(temporary_path, result.warnings);
        return result;
    }

    result.output_info = ffprobe_.probe(temporary_path.string(), runner,
        std::min(request.timeout, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(30))));
    if (!result.output_info.ok) {
        result.errors.emplace_back("Output validation failed: " +
            (result.output_info.errors.empty() ? std::string("unknown FFprobe error")
                                               : result.output_info.errors.front()));
    } else {
        const auto& actual = result.output_info.media;
        if (!format_matches(actual.container, request.plan.target_container))
            result.errors.emplace_back("Output container does not match the requested target.");
        if (count_kind(actual, MediaKind::Video) != count_kind(request.source_info, MediaKind::Video))
            result.errors.emplace_back("Output video stream count changed unexpectedly.");
        const auto source_audio = count_kind(request.source_info, MediaKind::Audio);
        const auto expected_audio = request.request.preserve_all_audio_tracks
            ? source_audio : std::min<std::size_t>(source_audio, 1U);
        if (count_kind(actual, MediaKind::Audio) != expected_audio)
            result.errors.emplace_back("Output audio stream count does not match the preservation policy.");
        const auto expected_subtitles = request.request.preserve_subtitles
            ? count_kind(request.source_info, MediaKind::Subtitle) : 0U;
        if (count_kind(actual, MediaKind::Subtitle) != expected_subtitles)
            result.errors.emplace_back("Output subtitle stream count does not match the preservation policy.");
        if (request.request.preserve_hdr && has_hdr(request.source_info) && !has_hdr(actual))
            result.errors.emplace_back("Output lost HDR signaling that was required to be preserved.");
        if (request.source_info.duration_ms && actual.duration_ms) {
            const auto source_duration = *request.source_info.duration_ms;
            const auto output_duration = *actual.duration_ms;
            const auto delta = source_duration > output_duration
                ? source_duration - output_duration : output_duration - source_duration;
            if (delta > 2000U) result.errors.emplace_back("Output duration differs from the source by more than two seconds.");
        }
    }
    if (!result.errors.empty()) {
        remove_if_present(temporary_path, result.warnings);
        return result;
    }

    std::error_code publish_error;
    std::filesystem::create_hard_link(temporary_path, final_path, publish_error);
    if (publish_error) {
        result.errors.emplace_back("Atomic no-overwrite publication failed: " + publish_error.message());
        remove_if_present(temporary_path, result.warnings);
        return result;
    }
    remove_if_present(temporary_path, result.warnings);
    result.output_path = final_path.string();
    result.ok = true;
    return result;
}
} // namespace telegram_media
