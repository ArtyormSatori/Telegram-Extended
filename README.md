# Telegram Extended — Universal Media & AI Suite

Cross-platform Telegram client extension concept with a shared media-processing core and an AI-assisted conversion planner.

## Project status

**M1 Shared Media Core foundation (in progress).** This repository contains the product requirements, technical design, a C++20 media planner, FFprobe metadata adapter, and a native process runner with bounded output and timeout handling. It is not yet a complete Telegram fork or production media-conversion product. AI model integration, conversion/job execution, native Telegram UI integration, and mobile applications remain future milestones.

## Supported product targets

- Windows 10 (desktop)
- Linux (desktop)
- macOS (desktop)
- Android
- iOS

## Build and test Shared Media Core

Requirements: CMake 3.20+, C++20 compiler, CTest. FFmpeg/FFprobe are optional; when both are found, CTest generates a synthetic MP4 and probes it end-to-end.

```bash
cmake -S core/media-core -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Implemented core components

- Deterministic conversion planner with remux-first behavior and policy validation.
- FFprobe flat-output parser for container, audio/video/subtitle streams, dimensions, duration, size, and HDR indicators.
- `NativeProcessRunner`: argv-based execution without a shell, process timeout, process-group/job termination, and per-stream output caps (8 MiB by default).
- `FFmpegCapabilityProbe`: discovers the installed binary's video/audio/subtitle encoders and muxer/demuxer formats at runtime.
- `FFmpegCommandBuilder`: creates shell-free, policy-checked FFmpeg argv for remux/transcode requests.
- `FFmpegConversionExecutor`: runs the command to a same-directory temporary output, probes and validates it, then publishes without overwriting an existing destination using an atomic hard link.
- Injectable `IProcessRunner` for test doubles and platform-specific implementations.
- Tests for planning, FFprobe parsing, real process launch, timeout, output limits, and optional FFmpeg/FFprobe integration.

The POSIX and Windows process-runner implementations are present in source. The current execution environment has only validated Linux; Windows/macOS must pass their GitHub Actions jobs before cross-platform validation can be claimed.

## Architecture

See [SRS](docs/SRS.md), [Technical Design](docs/TECHNICAL_DESIGN.md), [Process Runner design](docs/PROCESS_RUNNER.md), [Capability Registry](docs/CAPABILITY_REGISTRY.md), [FFmpeg Command Builder](docs/FFMPEG_COMMAND_BUILDER.md), [Conversion Executor](docs/FFMPEG_CONVERSION_EXECUTOR.md), and [ADRs](docs/ADRs.md).

## Safety boundaries

- Original files are immutable; outputs must use a separate destination.
- Media probing and conversion must run with bounded time, memory, file size, and concurrency.
- Network protocols in media inputs are disabled by default; only explicitly approved local files are accepted by the initial core.
- AI recommendations are proposals. The deterministic policy validator is authoritative.
- No media is uploaded to a remote processor or AI provider without explicit user consent.

## Licensing

The product must preserve upstream Telegram licensing and notices. FFmpeg build configuration, codecs, OCR/ASR models, and other dependencies require a separate license and redistribution review before release. See `docs/TECHNICAL_DESIGN.md`.
