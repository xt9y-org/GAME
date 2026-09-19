#ifndef GAME_RENDERING_FORWARD_PLUS_BENCHMARK_HPP
#define GAME_RENDERING_FORWARD_PLUS_BENCHMARK_HPP

#include "Benchmark.hpp"

namespace Rendering::ForwardPlusBenchmark {

using Samples = Benchmark::Samples;

struct Comparison {
    double full_loop_ms = 0.0;
    double forward_plus_ms = 0.0;

    double deltaMs() const
    {
        return Benchmark::deltaMs(full_loop_ms, forward_plus_ms);
    }

    double speedupPercent() const
    {
        return Benchmark::speedupPercent(full_loop_ms, forward_plus_ms);
    }
};

} // namespace Rendering::ForwardPlusBenchmark

#endif
