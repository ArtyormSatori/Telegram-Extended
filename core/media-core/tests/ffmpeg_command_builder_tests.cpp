#include "telegram_media/ffmpeg_command_builder.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace telegram_media;

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
MediaInfo sample_media() {
    MediaInfo info;
    info.container = "mp4";
    info.streams.push_back(StreamInfo{MediaKind::Video, "h264", 1920, 1080, {}, {}, false});
    info.streams.push_back(StreamInfo{MediaKind::Audio, "aac", {}, {}, 48000, 2, false});
    return info;
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / "telegram-extended-command-tests";
    std::filesystem::create_directories(root);
    const auto source = root / "source.mp4";
    const auto output = root / "output.mp4";
    { std::ofstream file(source); file << "fixture"; }
    std::error_code ignored;
    std::filesystem::remove(output, ignored);

    CodecCapabilities capabilities;
    capabilities.video_encoders.insert("libx264");
    capabilities.audio_encoders.insert("aac");
    capabilities.muxers.insert("mp4");
    capabilities.muxers.insert("matroska");

    auto media = sample_media();
    ConversionRequest request;
    request.target_container = "mp4";
    ConversionPlan remux;
    remux.operation = Operation::Remux;
    remux.target_container = "mp4";
    const auto copy = FFmpegCommandBuilder::build("ffmpeg", source.string(), output.string(),
                                                   media, request, remux, capabilities);
    expect(copy.ok, "valid remux command is built");
    expect(copy.arguments.size() > 5 && copy.arguments[1] == "-nostdin",
           "FFmpeg runs non-interactively");
    expect(std::find(copy.arguments.begin(), copy.arguments.end(), "-protocol_whitelist") != copy.arguments.end(),
           "network protocols are not enabled for local input");
    expect(std::find(copy.arguments.begin(), copy.arguments.end(), "copy") != copy.arguments.end(),
           "remux uses stream copy");
    expect(copy.arguments.back() == std::filesystem::absolute(output).string(),
           "output path is normalized and passed as a single argument");

    request.target_container = "matroska";
    request.target_video_codec = "libx264";
    request.target_audio_codec = "aac";
    ConversionPlan transcode;
    transcode.operation = Operation::Transcode;
    transcode.target_container = "matroska";
    transcode.video_codec = "libx264";
    transcode.audio_codec = "aac";
    const auto encode = FFmpegCommandBuilder::build("ffmpeg", source.string(), output.string(),
                                                     media, request, transcode, capabilities);
    expect(encode.ok, "valid transcode command is built");
    expect(std::find(encode.arguments.begin(), encode.arguments.end(), "libx264") != encode.arguments.end(),
           "requested video encoder is included");

    request.allow_transcode = false;
    const auto forbidden = FFmpegCommandBuilder::build("ffmpeg", source.string(), output.string(),
                                                        media, request, transcode, capabilities);
    expect(!forbidden.ok, "transcoding is rejected when user disabled it");
    request.allow_transcode = true;

    auto hdr = media;
    hdr.streams.front().hdr = true;
    request.preserve_hdr = true;
    const auto hdr_result = FFmpegCommandBuilder::build("ffmpeg", source.string(), output.string(),
                                                         hdr, request, transcode, capabilities);
    expect(!hdr_result.ok, "HDR transcode is blocked without an HDR-safe pipeline");

    ConversionRequest url_request;
    url_request.target_container = "mp4";
    const auto url_result = FFmpegCommandBuilder::build("ffmpeg", "https://example.invalid/a.mp4",
        output.string(), media, url_request, remux, capabilities);
    expect(!url_result.ok, "remote media URLs are rejected");

    std::filesystem::remove(source, ignored);
    std::filesystem::remove_all(root, ignored);
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "FFmpeg command builder tests passed.\n";
    return EXIT_SUCCESS;
}
