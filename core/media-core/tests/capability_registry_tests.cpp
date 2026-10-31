#include "telegram_media/capability_registry.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace telegram_media;

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
class FixtureRunner final : public IProcessRunner {
public:
    mutable int calls{0};
    ProcessResult run(const std::vector<std::string>& args,
                      std::chrono::milliseconds) const override {
        ++calls;
        ProcessResult result;
        result.exit_code = 0;
        if (args.back() == "-encoders") {
            result.standard_output =
                " V..... libx264 H.264 encoder\n"
                " V..... hevc_nvenc NVIDIA HEVC encoder\n"
                " A..... aac AAC encoder\n"
                " S..... ass ASS subtitle encoder\n";
        } else {
            result.standard_output =
                " DE matroska,webm Matroska / WebM\n"
                " E  mp4 MP4 (MPEG-4 Part 14)\n"
                " D  mpegts MPEG-TS\n";
        }
        return result;
    }
};
}

int main() {
    const auto parsed = CodecCapabilities::parse(
        " V..... libx264 H.264 encoder\n A..... aac AAC encoder\n S..... ass ASS encoder\n",
        " DE matroska,webm Matroska\n E  mp4 MP4\n D  mpegts MPEG-TS\n");
    expect(parsed.supports_video_encoder("libx264"), "video encoders are parsed");
    expect(parsed.supports_audio_encoder("aac"), "audio encoders are parsed");
    expect(parsed.subtitle_encoders.contains("ass"), "subtitle encoders are parsed");
    expect(parsed.supports_muxer("mp4"), "muxers are parsed");
    expect(parsed.supports_muxer("matroska"), "multi-name muxer aliases are parsed");
    expect(parsed.demuxers.contains("mpegts"), "demuxers are parsed");
    expect(!parsed.supports_video_encoder("not-a-codec"), "unknown codecs are not inferred");

    FixtureRunner runner;
    const auto inspected = FFmpegCapabilityProbe("ffmpeg").inspect(runner);
    expect(inspected.ok, "capability probe executes both FFmpeg discovery commands");
    expect(runner.calls == 2, "capability probe runs exactly two commands");
    expect(inspected.capabilities.supports_video_encoder("hevc_nvenc"),
           "capability probe returns discovered hardware encoder names");

#ifdef TELEGRAM_MEDIA_FFMPEG_EXECUTABLE
    NativeProcessRunner native_runner;
    const auto real = FFmpegCapabilityProbe(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE).inspect(native_runner);
    expect(real.ok, "real FFmpeg capability discovery succeeds when FFmpeg is installed");
    expect(real.capabilities.supports_muxer("mp4"), "real FFmpeg reports MP4 muxer");
#endif

    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Capability registry tests passed.\n";
    return EXIT_SUCCESS;
}
