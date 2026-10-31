#include "telegram_media/process_runner.hpp"
#include "telegram_media/ffprobe_adapter.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace telegram_media;

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    NativeProcessRunner runner;
    const auto echo = runner.run({TELEGRAM_MEDIA_CMAKE_COMMAND, "-E", "echo", "native-runner-ok"},
                                 std::chrono::seconds(5));
    expect(echo.launch_error.empty(), "native runner starts a real process");
    expect(echo.exit_code == 0, "native runner returns child exit status");
    expect(echo.standard_output.find("native-runner-ok") != std::string::npos,
           "native runner captures stdout");

    const auto timeout = runner.run({TELEGRAM_MEDIA_CMAKE_COMMAND, "-E", "sleep", "2"},
                                    std::chrono::milliseconds(100));
    expect(timeout.timed_out, "native runner enforces timeout");

    NativeProcessRunner bounded(8);
    const auto limited = bounded.run({TELEGRAM_MEDIA_CMAKE_COMMAND, "-E", "echo",
                                      "this-output-is-longer-than-eight-bytes"},
                                     std::chrono::seconds(5));
    expect(limited.output_limit_exceeded, "native runner enforces output limit");

    const auto invalid = runner.run({}, std::chrono::seconds(1));
    expect(!invalid.launch_error.empty(), "native runner rejects empty argv");

#if defined(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE) && defined(TELEGRAM_MEDIA_FFPROBE_EXECUTABLE)
    const auto fixture = std::filesystem::temp_directory_path() /
                         "telegram-extended-native-probe-fixture.mp4";
    const auto generated = runner.run({TELEGRAM_MEDIA_FFMPEG_EXECUTABLE,
        "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i",
        "color=c=black:s=160x90:d=1", "-f", "lavfi", "-i",
        "anullsrc=r=48000:cl=stereo", "-shortest", "-c:v", "mpeg4",
        "-c:a", "aac", "-y", fixture.string()}, std::chrono::seconds(30));
    expect(generated.exit_code == 0, "FFmpeg creates synthetic MP4 fixture");
    if (generated.exit_code == 0) {
        FFprobeAdapter adapter(TELEGRAM_MEDIA_FFPROBE_EXECUTABLE);
        const auto probe = adapter.probe(fixture.string(), runner, std::chrono::seconds(15));
        expect(probe.ok, "FFprobe inspects generated MP4 through native runner");
        expect(probe.media.streams.size() >= 2, "FFprobe returns video and audio streams");
    }
    std::error_code ignored;
    std::filesystem::remove(fixture, ignored);
#endif

    if (failures != 0) return EXIT_FAILURE;
    std::cout << "Native process runner tests passed.\n";
    return EXIT_SUCCESS;
}
