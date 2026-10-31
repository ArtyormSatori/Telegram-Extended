# FFmpeg Capability Registry — M1.3

The registry is populated from the installed FFmpeg binary at runtime. It parses `ffmpeg -hide_banner -encoders` and `ffmpeg -hide_banner -formats` output into separate video, audio, subtitle encoder, muxer, and demuxer sets. It does not assume that a codec exists because a platform or GPU model is known; callers must query the registry.

`FFmpegCapabilityProbe` invokes both discovery commands through `IProcessRunner`, so launch errors, timeouts, non-zero exit codes, and output-limit violations fail closed. The parser tolerates FFmpeg's one- or two-character format flags and comma-separated format aliases. Capability discovery is advisory input to plan generation; the execution backend must still validate the requested encoder/muxer combination before starting a conversion.

CTest covers fixture parsing, alias handling, unknown-codec rejection, injected-runner behavior, and real FFmpeg discovery when the executable is available. A registry entry only means FFmpeg reports the encoder or format in that binary; it does not prove that the hardware device, driver, profile, pixel format, or runtime license permits a particular encode.
