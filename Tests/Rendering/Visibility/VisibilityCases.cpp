#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Tests/Fixtures/SceneFixture.hpp"

#include "Renderer/Visibility/Visibility.hpp"

#include <algorithm>

namespace Tests {
namespace {

class VisibilityCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/visibility"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Models::clearCache();
        Ecs::World world;
        Testing::addCamera(world);
        const Testing::SceneAssets assets = Testing::triangleAssets();
        const Ecs::Entity visible = Testing::addTriangle(world, assets, {0.0f, 0.0f, -3.0f});
        const Ecs::Entity culled = Testing::addTriangle(world, assets, {100.0f, 0.0f, -3.0f});

        Renderer::Visibility::System visibility;
        const Renderer::Visibility::Result result = visibility.evaluate(world, 640, 360);
        return Testing::require(result.frustum.valid, "visibility frustum invalid", error) &&
            Testing::require(std::find(result.visible.begin(), result.visible.end(), visible) != result.visible.end(),
                             "visible entity was culled", error) &&
            Testing::require(std::find(result.culled.begin(), result.culled.end(), culled) != result.culled.end(),
                             "outside entity was visible", error) &&
            Testing::require(result.visible_triangles == 1u && result.culled_triangles == 1u,
                             "visibility triangle accounting mismatch", error);
    }
};

class VisibilityOverrideCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/visibility-override"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Visibility::System visibility;
        Renderer::Visibility::Result override_result;
        override_result.visible = {4u, 9u};
        override_result.culled = {2u};
        visibility.setOverride(override_result);
        if (!Testing::require(visibility.hasOverride(), "visibility override not enabled", error)) return false;
        visibility.clearOverride();
        return Testing::require(!visibility.hasOverride(), "visibility override not cleared", error);
    }
};

} // namespace

void registerVisibility(Testing::Runner& runner)
{
    runner.add<VisibilityCase>();
    runner.add<VisibilityOverrideCase>();
}

} // namespace Tests
