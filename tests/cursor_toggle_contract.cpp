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

static void requireToggle(const std::string& source)
{
    assert(source.find("Keyboard.KEY_TAB") != std::string::npos);
    assert(source.find("_tab_down") != std::string::npos);
    assert(source.find("tab_down && !_tab_down") != std::string::npos);
    assert(source.find("Mouse.isGrabbed()") != std::string::npos);
    assert(source.find("Mouse.setGrabbed") != std::string::npos);
    assert(source.find("Mouse.getDX()") != std::string::npos);
    assert(source.find("Mouse.getDY()") != std::string::npos);
}

int main()
{
    requireToggle(read("Examples/earth.cpp"));
    requireToggle(read("Examples/sponza.cpp"));
    return 0;
}
