#include <Rendering/VolumetricsBenchmark.hpp>

#include <cassert>
#include <cmath>

int main()
{
    using namespace Rendering::VolumetricsBenchmark;

    Samples samples;
    assert(samples.averageMs() == 0.0);
    assert(samples.medianMs() == 0.0);
    samples.add(7.0);
    samples.add(5.0);
    assert(std::abs(samples.averageMs() - 6.0) < 1.0e-9);
    assert(std::abs(samples.medianMs() - 6.0) < 1.0e-9);
    samples.add(6.0);
    assert(std::abs(samples.medianMs() - 6.0) < 1.0e-9);

    const Comparison overhead{10.0, 12.5};
    assert(std::abs(overhead.deltaMs() - 2.5) < 1.0e-9);
    assert(std::abs(overhead.overheadPercent() - 25.0) < 1.0e-9);

    const Comparison faster{10.0, 8.0};
    assert(std::abs(faster.deltaMs() + 2.0) < 1.0e-9);
    assert(std::abs(faster.overheadPercent() + 20.0) < 1.0e-9);

    const Comparison empty{};
    assert(empty.overheadPercent() == 0.0);
    return 0;
}
