#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Tests/Fixtures/SceneFixture.hpp"

#include "Renderer/Scenes/Scene.hpp"

#include <vector>

namespace Tests {
namespace {

class SceneCollectionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/scene-collection"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Models::clearCache();
        Ecs::World world;
        Testing::addCamera(world);
        const Testing::SceneAssets assets = Testing::triangleAssets();
        const Ecs::Entity entity = Testing::addTriangle(world, assets);

        std::vector<Renderer::Scenes::Scene::RenderItem> items;
        Renderer::Scenes::Scene::collectRenderItems(world, items);
        return Testing::require(items.size() == 1u, "render item count mismatch", error) &&
            Testing::require(items.front().entity == entity, "render item entity mismatch", error) &&
            Testing::require(items.front().mesh != nullptr && items.front().material != nullptr, "render item resources missing", error) &&
            Testing::require(items.front().mesh->indices.size() == 3u, "render item geometry mismatch", error);
    }
};

class SceneStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/scene-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity camera = Testing::addCamera(world);
        Testing::addLighting(world);
        const Renderer::Scenes::Scene::CameraState camera_state = Renderer::Scenes::Scene::cameraState(world);
        const Renderer::Scenes::Scene::LightState light_state = Renderer::Scenes::Scene::lightState(world);
        return Testing::require(camera_state.valid && camera_state.entity == camera, "camera scene state mismatch", error) &&
            Testing::require(light_state.valid && light_state.light.type == Renderer::LightType::Directional,
                             "light scene state mismatch", error);
    }
};

} // namespace

void registerScenes(Testing::Runner& runner)
{
    runner.add<SceneCollectionCase>();
    runner.add<SceneStateCase>();
}

} // namespace Tests
