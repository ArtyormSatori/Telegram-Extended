# Telegram Extended

Cross-platform Telegram client extension focused on professional media playback, conversion, and AI-assisted media intelligence.

## Project status

Bootstrap foundation (v0.1.0). This repository is not yet a complete Telegram fork and does not yet include a production FFmpeg execution adapter, AI model integration, or native mobile applications.

## Targets

- Windows 10 desktop
- Linux desktop
- macOS desktop
- Android
- iOS

## Build and test Shared Media Core

Requirements: CMake 3.20+ and a C++20 compiler.

```bash
cmake -S core/media-core -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

See [SRS](docs/SRS.md), [Technical Design](docs/TECHNICAL_DESIGN.md), and [ADRs](docs/ADRs.md).

## Safety and privacy

- Source files are never overwritten by default.
- Media processing must use bounded resources and least privilege.
- Remote processing requires explicit consent.
- AI proposes settings; deterministic policy validation controls execution.
- Review Telegram, FFmpeg, codec, model, and platform distribution licenses before release.