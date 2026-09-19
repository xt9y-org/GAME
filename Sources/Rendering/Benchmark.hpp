#ifndef GAME_RENDERING_BENCHMARK_HPP
#define GAME_RENDERING_BENCHMARK_HPP

#include <cstddef>

namespace Rendering::Benchmark {

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

inline double deltaMs(double baseline_ms, double candidate_ms)
{
    return candidate_ms - baseline_ms;
}

inline double speedupPercent(double baseline_ms, double candidate_ms)
{
    if (baseline_ms <= 0.0) return 0.0;
    return (baseline_ms - candidate_ms) / baseline_ms * 100.0;
}

} // namespace Rendering::Benchmark

#endif
