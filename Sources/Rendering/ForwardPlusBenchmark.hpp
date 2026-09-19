#ifndef GAME_RENDERING_FORWARD_PLUS_BENCHMARK_HPP
#define GAME_RENDERING_FORWARD_PLUS_BENCHMARK_HPP

#include <cstddef>

namespace Rendering::ForwardPlusBenchmark {

struct Samples {
    double total_ms = 0.0;
    std::size_t count = 0u;

    void add(double milliseconds)
    {
        total_ms += milliseconds;
        ++count;
    }

    double averageMs() const
    {
        return count == 0u ? 0.0 : total_ms / static_cast<double>(count);
    }
};

struct Comparison {
    double full_loop_ms = 0.0;
    double forward_plus_ms = 0.0;

    double deltaMs() const
    {
        return forward_plus_ms - full_loop_ms;
    }

    double speedupPercent() const
    {
        if (full_loop_ms <= 0.0) return 0.0;
        return (full_loop_ms - forward_plus_ms) / full_loop_ms * 100.0;
    }
};

} // namespace Rendering::ForwardPlusBenchmark

#endif
