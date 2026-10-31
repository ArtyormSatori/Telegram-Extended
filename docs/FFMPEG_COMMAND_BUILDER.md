# FFmpeg Command Builder — M1.4

The command builder converts a validated `ConversionPlan`, probed `MediaInfo`, user preservation constraints, and the runtime capability registry into an argv vector. It does not invoke a shell. The returned arguments can be passed directly to `IProcessRunner`.

Safety rules:

- Accept local file paths only; reject URL-style inputs and source overwrite requests.
- Require a probed audio/video source, an available muxer, and available explicitly requested encoders.
- Reject output paths that already exist and require the output directory to exist.
- Normalize paths and reject source/output aliases.
- Add `-nostdin`, `-xerror`, `-n`, and a `file`-only protocol whitelist.
- Prefer stream copy for remux; for transcode, only explicitly requested streams are encoded and compatible remaining streams are copied.
- Preserve all mapped tracks by default; optionally retain only the first audio track and preserve/drop subtitles according to the request.
- Block HDR transcoding until a separately validated HDR-preserving encode pipeline exists.

The builder only constructs commands. It does not execute the conversion, provide progress, create a temporary output, perform post-encode validation, or atomically publish a completed file. Those are required before a production conversion job can be exposed in the Telegram UI.
