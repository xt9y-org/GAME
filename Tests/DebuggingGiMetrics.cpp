#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

int main()
{
    const std::filesystem::path source =
        std::filesystem::path(__FILE__).parent_path().parent_path() /
        "Sources/Debugging/Debugging.cpp";
    std::ifstream file(source);
    assert(file);
    std::ostringstream stream;
    stream << file.rdbuf();
    const std::string text = stream.str();

    assert(text.find("Renderer/GlobalIllumination/Debug.hpp") != std::string::npos);
    assert(text.find("Global Illumination CPU") != std::string::npos);
    assert(text.find("Last Scene Sync") != std::string::npos);
    assert(text.find("Probe Update") != std::string::npos);
    assert(text.find("Last Photon Build") != std::string::npos);
    assert(text.find("features.global_illumination") != std::string::npos);

    return 0;
}
