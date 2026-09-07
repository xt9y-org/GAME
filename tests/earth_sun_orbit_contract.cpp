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
    assert(source.find("_sun_angle") != std::string::npos);
    assert(source.find("std::cos(_sun_angle)") != std::string::npos);
    assert(source.find("std::sin(_sun_angle)") != std::string::npos);
    assert(source.find("_sun_vertical_offset") != std::string::npos);
    assert(source.find("markChanged()") != std::string::npos);

    return 0;
}
