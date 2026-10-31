#include "telegram_media/ffmpeg_command_builder.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace telegram_media {
namespace {
bool is_local_path(const std::string& path) {
    return !path.empty() && path.find("://") == std::string::npos;
}
std::string normalize(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (!value.empty() && value.front() == '.') value.erase(value.begin());
    return value;
}
bool has_kind(const MediaInfo& info, MediaKind kind) {
    return std::any_of(info.streams.begin(), info.streams.end(),
        [kind](const StreamInfo& stream) { return stream.kind == kind; });
}
bool has_hdr(const MediaInfo& info) {
    return std::any_of(info.streams.begin(), info.streams.end(),
        [](const StreamInfo& stream) { return stream.kind == MediaKind::Video && stream.hdr; });
}
} // namespace

FFmpegCommandResult FFmpegCommandBuilder::build(
    const std::string& executable, const std::string& source_path,
    const std::string& output_path, const MediaInfo& source_info,
    const ConversionRequest& request, const ConversionPlan& plan,
    const CodecCapabilities& capabilities) {
    FFmpegCommandResult result;
    if (executable.empty()) result.errors.emplace_back("FFmpeg executable is required.");
    if (!is_local_path(source_path)) result.errors.emplace_back("A local source path is required.");
    if (!is_local_path(output_path)) result.errors.emplace_back("A local output path is required.");
    if (request.overwrite_source) result.errors.emplace_back("Overwriting the source file is forbidden.");
    if (request.target_container.empty() || plan.target_container.empty())
        result.errors.emplace_back("A target container is required.");
    if (normalize(request.target_container) != normalize(plan.target_container))
        result.errors.emplace_back("The validated plan container does not match the request.");
    if (plan.video_codec != request.target_video_codec || plan.audio_codec != request.target_audio_codec)
        result.errors.emplace_back("The validated plan codecs do not match the request.");
    if (source_path == output_path) result.errors.emplace_back("Source and output paths must be different.");
    if (plan.operation == Operation::Transcode && !request.allow_transcode)
        result.errors.emplace_back("Transcoding is disabled by the request.");
    if (source_info.streams.empty()) result.errors.emplace_back("Source media must be probed before conversion.");
    if (!capabilities.supports_muxer(plan.target_container))
        result.errors.emplace_back("The configured FFmpeg binary does not report the target muxer.");

    const bool video = has_kind(source_info, MediaKind::Video);
    const bool audio = has_kind(source_info, MediaKind::Audio);
    if (!video && !audio) result.errors.emplace_back("FFmpeg media conversion requires an audio or video stream.");
    if (plan.video_codec && (!video || !capabilities.supports_video_encoder(*plan.video_codec)))
        result.errors.emplace_back("The requested video encoder is unavailable or the source has no video stream.");
    if (plan.audio_codec && (!audio || !capabilities.supports_audio_encoder(*plan.audio_codec)))
        result.errors.emplace_back("The requested audio encoder is unavailable or the source has no audio stream.");
    if (plan.operation == Operation::Remux && (plan.video_codec || plan.audio_codec))
        result.errors.emplace_back("A remux plan cannot request encoder changes.");
    if (plan.operation == Operation::Transcode && video && !plan.video_codec)
        result.errors.emplace_back("A video transcode must specify a target video codec.");
    if (plan.operation == Operation::Transcode && !video && audio && !plan.audio_codec)
        result.errors.emplace_back("An audio-only transcode must specify a target audio codec.");
    if (request.preserve_hdr && has_hdr(source_info) && plan.operation == Operation::Transcode)
        result.errors.emplace_back("HDR transcoding is blocked until an HDR-preserving pipeline is configured.");

    std::error_code path_error;
    if (!source_path.empty() && !std::filesystem::is_regular_file(source_path, path_error))
        result.errors.emplace_back("Source path must refer to an existing regular file.");
    if (path_error) result.errors.emplace_back("Could not inspect source path: " + path_error.message());
    path_error.clear();
    if (!output_path.empty() && std::filesystem::exists(output_path, path_error))
        result.errors.emplace_back("Output file already exists; refusing to overwrite it.");
    if (path_error) result.errors.emplace_back("Could not inspect output path: " + path_error.message());
    path_error.clear();
    if (!output_path.empty()) {
        const auto parent = std::filesystem::path(output_path).parent_path();
        if (!parent.empty() && !std::filesystem::is_directory(parent, path_error))
            result.errors.emplace_back("Output directory does not exist.");
        if (path_error) result.errors.emplace_back("Could not inspect output directory: " + path_error.message());
    }
    if (!result.errors.empty()) return result;

    const auto source_absolute = std::filesystem::weakly_canonical(source_path, path_error);
    if (path_error) { result.errors.emplace_back("Could not normalize source path: " + path_error.message()); return result; }
    const auto output_absolute = std::filesystem::weakly_canonical(output_path, path_error);
    if (path_error) { result.errors.emplace_back("Could not normalize output path: " + path_error.message()); return result; }
    const auto source = source_absolute.string();
    const auto output = output_absolute.string();
    if (source == output) {
        result.errors.emplace_back("Normalized source and output paths must be different.");
        return result;
    }

    auto& args = result.arguments;
    args = {executable, "-nostdin", "-hide_banner", "-v", "error", "-xerror",
            "-protocol_whitelist", "file", "-i", source};
    if (request.preserve_all_audio_tracks) {
        args.insert(args.end(), {"-map", "0"});
    } else {
        args.insert(args.end(), {"-map", "0:v?", "-map", "0:a:0?"});
        if (request.preserve_subtitles) args.insert(args.end(), {"-map", "0:s?"});
        args.insert(args.end(), {"-map", "0:d?", "-map", "0:t?"});
    }
    args.insert(args.end(), {"-map_metadata", "0", "-map_chapters", "0"});
    if (plan.operation == Operation::Remux) {
        args.insert(args.end(), {"-c", "copy"});
    } else {
        if (video) args.insert(args.end(), {"-c:v", plan.video_codec.value_or("copy")});
        if (audio) args.insert(args.end(), {"-c:a", plan.audio_codec.value_or("copy")});
        if (request.preserve_subtitles) args.insert(args.end(), {"-c:s", "copy"});
        else args.emplace_back("-sn");
        args.insert(args.end(), {"-c:d", "copy", "-c:t", "copy"});
    }
    if (!request.preserve_subtitles && plan.operation == Operation::Remux) args.emplace_back("-sn");
    if (normalize(plan.target_container) == "mp4" || normalize(plan.target_container) == "mov")
        args.insert(args.end(), {"-movflags", "+faststart"});
    args.insert(args.end(), {"-f", normalize(plan.target_container), "-n"});
    args.push_back(output);
    result.ok = true;
    return result;
}
} // namespace telegram_media
