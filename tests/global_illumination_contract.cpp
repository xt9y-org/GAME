#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

static std::string read(const char *path)
{
    std::ifstream file(path);
    assert(file.good());
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

int main()
{
    const std::string earth = read("Examples/earth.cpp");
    const std::string sponza = read("Examples/sponza.cpp");

    assert(earth.find("settings.max_bounces") == std::string::npos);
    assert(earth.find("GlobalIlluminationComponent") != std::string::npos);
    assert(sponza.find("GlobalIlluminationComponent") != std::string::npos);
    return 0;
}
