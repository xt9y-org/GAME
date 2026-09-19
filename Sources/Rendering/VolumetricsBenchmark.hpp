#ifndef GAME_RENDERING_VOLUMETRICS_BENCHMARK_HPP
#define GAME_RENDERING_VOLUMETRICS_BENCHMARK_HPP

#include "Benchmark.hpp"

namespace Rendering::VolumetricsBenchmark {

using Samples = Benchmark::Samples;

struct Comparison {
    double disabled_ms = 0.0;
    double enabled_ms = 0.0;

    double deltaMs() const
    {
        return Benchmark::deltaMs(disabled_ms, enabled_ms);
    }

    double overheadPercent() const
    {
        if (disabled_ms <= 0.0) return 0.0;
        return (enabled_ms - disabled_ms) / disabled_ms * 100.0;
    }
};

} // namespace Rendering::VolumetricsBenchmark

#endif
