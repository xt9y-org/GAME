#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
    std::ifstream input("Examples/earth.cpp");
    assert(input.good());

    const std::string source(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    assert(source.find("_sun_orbit_radius") != std::string::npos);
    assert(source.find("_sun_vertical_offset") != std::string::npos);

    // The Earth light is intentionally static for now so the path tracer can
    // accumulate instead of resetting because the light signature changes.
    assert(source.find("_sun_angle") == std::string::npos);
    assert(source.find("std::fmod") == std::string::npos);
    assert(source.find("std::cos(") == std::string::npos);
    assert(source.find("std::sin(") == std::string::npos);

    // Earth is cheap enough to render at a higher stationary quality than the
    // heavy Sponza scene.
    assert(source.find("resolution_divisor = 2") != std::string::npos);
    assert(source.find("samples_per_frame = 2") != std::string::npos);
    assert(source.find("max_bounces = 1") != std::string::npos);

    // The source FBX is intentionally retained as an importer regression and
    // size reference, but its 512-triangle shell is not suitable as the final
    // presentation globe. The demo must render the dense seamless sphere via
    // Horse's generic runtime mesh/material registry.
    assert(source.find("EarthDemo::makeSphere") != std::string::npos);
    assert(source.find("Models::registerMesh") != std::string::npos);
    assert(source.find("Models::registerMaterial") != std::string::npos);
    assert(source.find("_display_radius") != std::string::npos);

    return 0;
}
