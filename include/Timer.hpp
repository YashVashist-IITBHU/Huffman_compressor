#pragma once

#include <chrono>

namespace huff {

// Small RAII-free stopwatch built on the monotonic clock.
class Timer {
public:
    Timer() : start_(Clock::now()) {}
    void reset() { start_ = Clock::now(); }
    double elapsedSeconds() const {
        return std::chrono::duration<double>(Clock::now() - start_).count();
    }
    double elapsedMillis() const { return elapsedSeconds() * 1000.0; }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
};

}  // namespace huff
