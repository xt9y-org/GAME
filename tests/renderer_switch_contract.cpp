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

static void checkExample(const char *path)
{
    const std::string source = read(path);

    assert(source.find("Renderer::Rasterizer *rasterizer_") != std::string::npos);
    assert(source.find("Renderer::PathTracer *path_tracer_") != std::string::npos);
    assert(source.find("#define RAST") == std::string::npos);
    assert(source.find("Keyboard.KEY_RETURN") != std::string::npos);
    assert(source.find("RenderTechnique::PathTracer") != std::string::npos);
    assert(source.find("camera_moving") != std::string::npos);
    assert(source.find("Rasterizer (moving)") != std::string::npos);
    assert(source.find("Technique: ") != std::string::npos);

    assert(source.find("metal_surface_active_") != std::string::npos);
    assert(source.find("lwmglSurfaceDetach") != std::string::npos);
    assert(source.find("lwmglSurfaceAttach") != std::string::npos);
    assert(source.find("Metal.waitIdle") != std::string::npos);
    assert(source.find("setPathTracerSurface") != std::string::npos);
    assert(source.find("if (e->metal_surface_active_)") != std::string::npos);
}

int main()
{
    checkExample("Examples/earth.cpp");
    checkExample("Examples/sponza.cpp");
    return 0;
}
