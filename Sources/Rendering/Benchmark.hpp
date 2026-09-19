#ifndef GAME_RENDERING_BENCHMARK_HPP
#define GAME_RENDERING_BENCHMARK_HPP

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Rendering::Benchmark {

struct Samples {
    double total_ms = 0.0;
    std::size_t count = 0u;
    std::vector<double> values;

    void add(double milliseconds)
    {
        total_ms += milliseconds;
        ++count;
        values.push_back(milliseconds);
    }

    double averageMs() const
    {
        return count == 0u ? 0.0 : total_ms / static_cast<double>(count);
    }

    double medianMs() const
    {
        if (values.empty()) return 0.0;

        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t middle = sorted.size() / 2u;
        if ((sorted.size() & 1u) != 0u) return sorted[middle];
        return (sorted[middle - 1u] + sorted[middle]) * 0.5;
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
