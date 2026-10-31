#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace telegram_media {
enum class MediaKind { Unknown, Video, Audio, Image, Subtitle };
enum class Operation { Remux, Transcode };
enum class QualityMode { Preserve, Balanced, Smallest, Custom };
struct StreamInfo {
    MediaKind kind{MediaKind::Unknown};
    std::string codec;
    std::optional<std::uint32_t> width;
    std::optional<std::uint32_t> height;
    std::optional<std::uint32_t> sample_rate;
    std::optional<std::uint32_t> channels;
    bool hdr{false};
};
struct MediaInfo {
    std::string container;
    std::vector<StreamInfo> streams;
    std::optional<std::uint64_t> duration_ms;
    std::optional<std::uint64_t> size_bytes;
};
struct ConversionRequest {
    std::string target_container;
    std::optional<std::string> target_video_codec;
    std::optional<std::string> target_audio_codec;
    QualityMode quality{QualityMode::Balanced};
    bool preserve_all_audio_tracks{true};
    bool preserve_subtitles{true};
    bool preserve_hdr{true};
    bool allow_transcode{true};
    bool overwrite_source{false};
};
struct ConversionPlan {
    Operation operation{Operation::Transcode};
    std::string target_container;
    std::optional<std::string> video_codec;
    std::optional<std::string> audio_codec;
    std::vector<std::string> rationale;
};
struct PlanResult { bool ok{false}; ConversionPlan plan; std::vector<std::string> errors; };
} // namespace telegram_media
