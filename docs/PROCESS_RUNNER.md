# Native Process Runner — M1.2

## Purpose

Provide a single bounded process-execution contract for FFprobe now and FFmpeg backends later. The runner is infrastructure only; it does not implement transcoding, job scheduling, progress persistence, or atomic output publication.

## Contract

`IProcessRunner::run(argv, timeout)` returns:

- child exit code;
- captured stdout and stderr;
- timeout status;
- output-limit status;
- a launch/configuration error string.

The native implementation rejects an empty executable, non-positive timeout, zero output budget, NUL-containing arguments, and argument vectors exceeding 1 MiB. The default capture budget is 8 MiB per output stream. Arguments are passed directly to the OS process API; no shell is invoked.

## Platform behavior

- POSIX: `fork` + `execvp`, non-blocking pipes, `poll`, and a dedicated process group. Timeout/output overflow sends SIGKILL to the process group.
- Windows: `CreateProcessW`, correctly quoted UTF-16 command line, redirected pipes drained concurrently, and a Job Object configured to terminate assigned descendants when the job handle closes. Direct process termination remains the fallback if job assignment is unavailable.

## FFprobe integration

The FFprobe adapter constructs an argv vector and invokes the injected runner. Empty paths, launch errors, timeouts, and non-zero process exit are returned as probe errors. Successful output is parsed from FFprobe's flat format.

## Tests

CTest includes separate planner/parser and native process-runner suites. Runner tests launch CMake's portable helper process, verify stdout and exit code, enforce a short timeout, and enforce an output cap. When both FFmpeg and FFprobe are installed, an additional test creates a synthetic MP4 and probes it through the real adapter.

## Validation boundary

Linux has been built and tested locally with strict compiler warnings treated as errors. Windows and macOS jobs are configured in GitHub Actions but must complete successfully before the code is declared cross-platform validated. The process runner is not a security sandbox: production media workers still need OS-level privilege isolation, resource limits, network restrictions, and filesystem policy.
