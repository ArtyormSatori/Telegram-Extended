# FFmpeg Conversion Executor — M1.5

`FFmpegConversionExecutor` executes the shell-free argv from `FFmpegCommandBuilder` through the injected `IProcessRunner`. It writes to a uniquely named temporary file in the destination directory, refuses overwrite, enforces the job timeout, probes the completed temporary file, and checks the target container, video/audio/subtitle stream counts, duration tolerance, and required HDR signaling.

Publication uses a same-directory hard link from the validated temporary file to the final destination. This operation is atomic and fails if the destination already exists; the temporary link is then removed. If the filesystem does not support hard links, publication fails closed instead of falling back to a non-atomic overwrite-prone copy. A cleanup warning is returned if the temporary name cannot be removed after publication.

CTest runs an end-to-end synthetic MP4 remux through FFmpeg and FFprobe when both executables are installed. It checks that the final output is present, source remains present, and a second attempt cannot overwrite the destination.

Remaining limitations: no progress events, pause/resume, cancellation, persistent queue, resource quotas beyond process timeout/output limits, crash recovery, fsync durability guarantee, or Telegram UI integration. These remain mandatory before production release.
