#include "telegram_media/ffmpeg_conversion_executor.hpp"
#include "telegram_media/planner.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace telegram_media;

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
#if defined(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE) && defined(TELEGRAM_MEDIA_FFPROBE_EXECUTABLE)
    NativeProcessRunner runner;
    const auto root = std::filesystem::temp_directory_path() / "telegram-extended-executor-tests";
    std::filesystem::create_directories(root);
    const auto source = root / "input.mp4";
    const auto output = root / "output.mp4";
    std::error_code ignored;
    std::filesystem::remove(source, ignored);
    std::filesystem::remove(output, ignored);

    const auto generated = runner.run({TELEGRAM_MEDIA_FFMPEG_EXECUTABLE,
        "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i",
        "color=c=black:s=160x90:d=1", "-f", "lavfi", "-i",
        "anullsrc=r=48000:cl=stereo", "-shortest", "-c:v", "mpeg4",
        "-c:a", "aac", "-y", source.string()}, std::chrono::seconds(30));
    expect(generated.exit_code == 0, "test input media is generated");

    FFprobeAdapter probe(TELEGRAM_MEDIA_FFPROBE_EXECUTABLE);
    const auto source_info = probe.probe(source.string(), runner, std::chrono::seconds(15));
    expect(source_info.ok, "test input media is probed");

    const auto capabilities = FFmpegCapabilityProbe(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE).inspect(runner);
    expect(capabilities.ok, "runtime FFmpeg capabilities are discovered");

    if (generated.exit_code == 0 && source_info.ok && capabilities.ok) {
        ConversionRequest conversion;
        conversion.target_container = "mp4";
        const auto plan = create_plan(source_info.media, conversion);
        expect(plan.ok && plan.plan.operation == Operation::Remux,
               "container aliases select a lossless remux plan");
        if (plan.ok) {
            ConversionExecutionRequest job;
            job.source_path = source.string();
            job.output_path = output.string();
            job.request = conversion;
            job.plan = plan.plan;
            job.timeout = std::chrono::seconds(30);
            FFmpegConversionExecutor executor(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE,
                                               TELEGRAM_MEDIA_FFPROBE_EXECUTABLE);
            const auto result = executor.execute(job, capabilities.capabilities, runner);
            expect(result.ok, "executor remuxes, probes, and publishes the MP4 output");
            expect(std::filesystem::is_regular_file(output), "published output exists");
            expect(std::filesystem::is_regular_file(source), "source remains immutable");
            const auto second = executor.execute(job, capabilities.capabilities, runner);
            expect(!second.ok, "existing destination is never overwritten");
        }
    }
    std::filesystem::remove_all(root, ignored);
#endif
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "FFmpeg conversion executor tests passed.\n";
    return EXIT_SUCCESS;
}
