#include <Rendering/GiBenchmarkScene.hpp>

#include <cassert>
#include <filesystem>
#include <string>

int main()
{
    using namespace Rendering::GiBenchmarkScene;

    const std::filesystem::path fallback = "Assets/Sponza/sponza.obj";

    const Selection argument = select("scene.gltf", "environment.gltf", fallback, true);
    assert(argument.path == std::filesystem::path("scene.gltf"));
    assert(argument.source == Source::Argument);
    assert(!argument.generated());

    const Selection environment = select({}, "environment.gltf", fallback, true);
    assert(environment.path == std::filesystem::path("environment.gltf"));
    assert(environment.source == Source::Environment);
    assert(!environment.generated());

    const Selection default_scene = select({}, {}, fallback, true);
    assert(default_scene.path == fallback);
    assert(default_scene.source == Source::Default);
    assert(!default_scene.generated());

    const Selection synthetic = select({}, {}, fallback, false);
    assert(synthetic.path.empty());
    assert(synthetic.source == Source::Synthetic);
    assert(synthetic.generated());

    assert(std::string(sourceName(Source::Argument)) == "argument");
    assert(std::string(sourceName(Source::Environment)) == "environment");
    assert(std::string(sourceName(Source::Default)) == "default");
    assert(std::string(sourceName(Source::Synthetic)) == "synthetic");
    return 0;
}
