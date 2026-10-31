# Technical Design — Telegram Extended Media Platform

**Version:** 0.1.0 · Foundation design

## Principles
Local-first; remote processing by explicit consent; shared domain contracts with native platform adapters; AI proposes but deterministic policy authorizes; immutable inputs and atomic outputs; safe remux before transcode; isolate Telegram upstream changes.

## Logical architecture
```text
Telegram UI (Desktop / Android / iOS)
              |
       Platform Adapter
              |
        Shared Media API
              |
    Job Manager / Policy Engine
       |       |        |
     Probe   Planner  Validator
       |       |        |
    FFprobe  AI Gateway  Capability Registry
       |
   Media Backend Adapters (FFmpeg / image / subtitle)
       |
   Restricted Worker + Atomic Output
```

## Shared Media Core
The initial core is dependency-light C++20. It owns domain types, conversion requests, plans and deterministic validation. It must not depend on Qt, Telegram internals, Python, network services or UI frameworks. Public concepts: MediaInfo, ConversionRequest, ConversionPlan, PlanResult, create_plan and validate_plan. A future C ABI/FFI must define ownership, errors, cancellation and versioning before being frozen.

## Media pipeline
1. Validate local path and file size.
2. Identify by signature, not extension.
3. Probe through restricted FFprobe adapter.
4. Resolve intent and device capabilities.
5. Build candidate remux/transcode plans.
6. Validate against capabilities and user constraints.
7. Optionally encode a sample for quality/size estimates.
8. Execute with bounded time, memory, CPU and output size.
9. Validate streams, decoding, duration, sync and integrity.
10. Publish atomically; retain source unchanged.

FFmpeg must not expose unnecessary network protocols. First implementation accepts local files only. Never pass user text directly to a shell; use process APIs with argument arrays.

## AI optimizer
Intent parser, media analyzer, capability resolver, candidate planner, scoring engine and explanation renderer. AI output is untrusted: validate schema and deterministic policy. If model unavailable, fall back to deterministic presets. Score compatibility, quality, size, speed, power, track preservation and user constraints. No silent destructive trade-offs.

## Job lifecycle
Created → Probing → Planned → AwaitingApproval (when required) → Queued → Running → Validating → Succeeded. Terminal states: Failed and Cancelled. Persist transitions; redact sensitive paths. Use per-job temporary directories and atomic rename where supported; clean partial files after failure/cancellation.

## Platform strategy
Windows: C++ core and desktop integration; evaluate Media Foundation, NVENC and QSV. Linux: VAAPI, NVENC and QSV. macOS: VideoToolbox/AVFoundation. Android: Kotlin, MediaCodec and JNI where appropriate. iOS: Swift/UIKit and AVFoundation/VideoToolbox; respect background and App Store constraints. Telegram Desktop, Android and iOS are separate upstream codebases.

## CI/CD
Build and test on Windows, Ubuntu and macOS. Add sanitizer builds, synthetic media integration tests, dependency/license inventory, SBOM, signed builds, provenance and platform QA before release.

## Security and privacy
Treat media as untrusted. Least privilege, bounded memory/time/size/concurrency, safe temp paths, no network by default, redacted logs and explicit remote consent. Validate output before publication.

## Risks
Codec patents and redistribution obligations vary; FFmpeg LGPL/GPL configuration changes obligations; hardware encoders depend on drivers; iOS background work is constrained; Telegram upstream changes require ongoing integration testing.


## Native process execution (M1.2)

`NativeProcessRunner` implements `IProcessRunner` using `fork`/`execvp` and non-blocking pipes on POSIX, and `CreateProcessW` with inherited stdout/stderr pipes on Windows. Arguments are passed as an argv vector and never interpreted by a shell. It rejects empty executables, NUL-containing arguments, argument vectors larger than 1 MiB, non-positive timeouts, and zero output budgets. Standard output and standard error are independently bounded (8 MiB by default); exceeding a bound terminates the process. POSIX children are placed in a process group so timeout termination also targets descendants. Windows uses a Job Object with kill-on-close where the host permits job assignment, with direct-process termination as a fallback.

The runner returns structured launch errors, exit status, timeout state, and output-limit state. FFprobeAdapter propagates launch failures and rejects timed-out, non-zero, or oversized probe output. Tests cover real child process launch, timeout, output caps, invalid argv, and (when FFmpeg and FFprobe are installed) a generated MP4 probed through the actual adapter.

Known validation boundary: the current execution environment is Linux. The Windows and macOS CI matrix is configured, but their results must be confirmed from GitHub Actions before the implementation is declared cross-platform validated. This process runner is an infrastructure primitive; it does not itself implement FFmpeg transcoding, job scheduling, or output publication.


## Runtime codec and container capability registry (M1.3)

`FFmpegCapabilityProbe` discovers encoders and formats from the configured FFmpeg executable through the bounded process runner. The registry stores video/audio/subtitle encoder names and demuxer/muxer names separately, including comma-separated format aliases. Unknown codecs are never inferred. This registry is advisory: a listed encoder does not guarantee that a specific hardware device, driver, profile, pixel format, or license permits a particular operation. The final execution backend must validate the concrete stream/device combination and probe the produced output.


## FFmpeg command construction (M1.4)

`FFmpegCommandBuilder` combines the user request, source metadata, validated conversion plan, and runtime capability registry into an argv vector. It refuses URL-style inputs, source overwrite, output collisions, unsupported muxers/encoders, mismatched plan settings, and HDR transcodes without an HDR-safe pipeline. It requests non-interactive execution, file-only protocols, error-on-decode-failure, stream mapping, and no-overwrite behavior. The builder is not yet an execution/job adapter: progress, temporary outputs, post-encode QA, and atomic publication remain unimplemented.


## FFmpeg conversion execution (M1.5)

`FFmpegConversionExecutor` invokes the command builder through `IProcessRunner`, writes to a unique temporary output in the destination directory, probes the result, validates container/stream counts/duration/HDR policy, and publishes with a same-directory hard link that fails if the destination exists. This is atomic no-overwrite publication; filesystems without hard-link support fail closed. Progress, pause/resume, cancellation, persistent job state, resource quotas, crash recovery, fsync durability, and UI integration are still absent.
