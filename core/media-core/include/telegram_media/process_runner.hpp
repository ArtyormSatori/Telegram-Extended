#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace telegram_media {

struct ProcessResult {
    int exit_code{-1};
    std::string standard_output;
    std::string standard_error;
    bool timed_out{false};
    bool output_limit_exceeded{false};
    std::string launch_error;
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    [[nodiscard]] virtual ProcessResult run(
        const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) const = 0;
};

class NativeProcessRunner final : public IProcessRunner {
public:
    explicit NativeProcessRunner(std::size_t max_output_bytes = 8U * 1024U * 1024U);
    [[nodiscard]] ProcessResult run(
        const std::vector<std::string>& arguments,
        std::chrono::milliseconds timeout) const override;
private:
    std::size_t max_output_bytes_;
};

} // namespace telegram_media
