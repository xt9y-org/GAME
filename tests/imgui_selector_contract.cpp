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
    const std::string build = read("build.c");
    const std::string earth = read("Examples/earth.cpp");
    const std::string sponza = read("Examples/sponza.cpp");

    assert(build.find("https://github.com/ocornut/imgui.git") != std::string::npos);
    assert(build.find("v1.92.9b") != std::string::npos);
    assert(build.find("c_dep_header_only(imgui)") != std::string::npos);
    assert(build.find("c_dep_include(imgui, \".\")") != std::string::npos);

    for (const std::string *source : {&earth, &sponza}) {
        assert(source->find("#include <imgui.h>") != std::string::npos);
        assert(source->find("#include \"Sources/UI/UI.hpp\"") != std::string::npos);
        assert(source->find("UI::init()") != std::string::npos);
        assert(source->find("UI::beginFrame()") != std::string::npos);
        assert(source->find("UI::rendererSelector") != std::string::npos);
        assert(source->find("UI::wantsMouse()") != std::string::npos);
        assert(source->find("UI::wantsKeyboard()") != std::string::npos);
        assert(source->find("UI::shutdown()") != std::string::npos);
    }
    return 0;
}
