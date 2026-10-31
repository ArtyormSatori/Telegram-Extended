#pragma once
#include <chrono>
#include <string>
#include <vector>
namespace telegram_media {
struct ProcessResult {
    int exit_code{-1};
    std::string standard_output;
    std::string standard_error;
    bool timed_out{false};
};
// Implementations must use argv-based process creation, never a shell,
// and enforce the supplied timeout.
class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    [[nodiscard]] virtual ProcessResult run(
        const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) const = 0;
};
} // namespace telegram_media
