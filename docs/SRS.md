# Software Requirements Specification

**Product:** Telegram Extended — Universal Media Converter AI Professional Edition  
**Version:** 0.1.0  
**Status:** Draft baseline

## Purpose
Extend Telegram clients with professional media inspection, playback, conversion, batch processing and AI-assisted optimization while preserving Telegram accounts, chats, groups, channels, calls, contacts and synchronization.

## Scope
- Inspect files by signature and probe containers/streams; never trust extensions alone.
- Support video, audio, image and subtitle workflows through replaceable backends.
- Prefer remux only when target container and stream compatibility are verified.
- Transcode with explicit codec, quality, bitrate, frame-rate, resolution, color, audio and subtitle policies.
- Batch queues, progress, cancellation, history, logs and safe recovery.
- AI intent analysis and explainable plans, followed by deterministic policy validation.
- Local processing by default; remote processing only with explicit per-operation consent.
- Shared contracts for Windows, Linux, macOS, Android and iOS clients.

## Functional requirements
| ID | Requirement | Priority |
|---|---|---|
| FR-001 | Detect media by magic bytes/MIME and probe streams. | MUST |
| FR-002 | Report container, codecs, duration, dimensions, FPS, bitrate, audio, HDR/color and subtitles where available. | MUST |
| FR-003 | Support video/audio/image/subtitle operations via pluggable backends. | MUST |
| FR-004 | Select remux only after compatibility is verified. | MUST |
| FR-005 | Represent explicit stream preservation and transcode policies. | MUST |
| FR-006 | Never overwrite sources; publish outputs atomically after validation. | MUST |
| FR-007 | Bounded batch queue, progress, cancellation and per-job logs. | MUST |
| FR-008 | Persist job state and recover safely after interruption. | SHOULD |
| FR-009 | AI returns structured recommendations, rationale and trade-offs. | MUST |
| FR-010 | Deterministic policy validates plans against capabilities and constraints. | MUST |
| FR-011 | Preserve or warn about HDR, color metadata, audio tracks, chapters and subtitles. | MUST |
| FR-012 | Presets for Telegram, mobile, web, social, archive, high quality and small size. | SHOULD |
| FR-013 | Trim, split, merge, audio/frame extraction, normalization and subtitle tools. | SHOULD |
| FR-014 | Runtime hardware encoder detection with CPU fallback. | SHOULD |
| FR-015 | Remote AI/processing requires explicit consent and disclosure. | MUST |
| FR-016 | Offline local probe and conversion mode. | MUST |
| FR-017 | Validate output decode, streams, duration and integrity. | MUST |
| FR-018 | Stable API contracts for desktop and mobile clients. | MUST |

## Non-functional requirements
Portability across target platforms; reliability without source corruption; least-privilege sandboxing and bounded resources; local-first privacy; streaming I/O; accessible localized UI; redacted structured logs; modular maintainable core; no Telegram feature regressions; license and patent review before distribution.

## Media coverage
Target containers: MP4, MOV, MKV, AVI, WebM, MPEG-TS, M2TS/MTS and FLV. Audio: MP3, AAC, M4A, WAV, FLAC, Opus, OGG and ALAC. Images: JPEG, PNG, WebP, AVIF, TIFF and BMP. Subtitles: SRT, WebVTT, ASS/SSA and embedded tracks. Actual support depends on backend build and platform; unsupported codecs must be disclosed.

## AI optimization
Inputs: user intent, probe result, device capabilities, encoders and destination constraints. Outputs: structured plan, rationale, warnings, alternatives and estimated size/time ranges. Reject unknown encoders, invalid parameters, unsafe paths, prohibited protocols, missing consent and violations of user constraints. Never silently drop tracks, alter HDR/SDR, lower resolution or overwrite a source.

## Acceptance criteria
Core tests pass on Windows/Linux/macOS; synthetic or licensed integration fixtures probe and decode; remux is chosen only with verified compatibility; source hashes remain unchanged; cancellation never publishes partial outputs; AI plans pass schema and policy checks; local-only mode makes no network requests; mobile parity is claimed only after native build/device validation.
