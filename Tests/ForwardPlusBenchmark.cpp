#include <Rendering/ForwardPlusBenchmark.hpp>

#include <cassert>
#include <cmath>

int main()
{
    using namespace Rendering::ForwardPlusBenchmark;

    Samples samples;
    assert(samples.averageMs() == 0.0);
    samples.add(10.0);
    samples.add(14.0);
    assert(std::abs(samples.averageMs() - 12.0) < 1.0e-9);

    const Comparison faster{10.0, 8.0};
    assert(std::abs(faster.deltaMs() + 2.0) < 1.0e-9);
    assert(std::abs(faster.speedupPercent() - 20.0) < 1.0e-9);

    const Comparison slower{10.0, 12.5};
    assert(std::abs(slower.deltaMs() - 2.5) < 1.0e-9);
    assert(std::abs(slower.speedupPercent() + 25.0) < 1.0e-9);

    const Comparison empty{};
    assert(empty.speedupPercent() == 0.0);
    return 0;
}
