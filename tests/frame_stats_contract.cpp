#include "Examples/earth.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

int main()
{
    const Models::MeshData mesh = EarthDemo::makeSphere(1.0f);
    const std::size_t triangles = mesh.indices.size() / 3u;
    assert(triangles == 65024u);

    const double delta = 1.0 / 144.0;
    const long fps = delta > 1.0e-6 ? std::lround(1.0 / delta) : 0L;
    assert(fps == 144L);
    return 0;
}
