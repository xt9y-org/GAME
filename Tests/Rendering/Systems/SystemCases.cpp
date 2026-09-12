#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Renderer/Components.hpp"
#include "Renderer/Debug/Debug.hpp"
#include "Renderer/Environment.hpp"
#include "Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Renderer/GlobalIllumination/PhotonMapping/PhotonMap.hpp"
#include "Renderer/Lod.hpp"
#include "Renderer/Manager.hpp"

namespace Tests {
namespace {

class MockRenderer final : public Renderer::IRenderer {
public:
    bool init() override
    {
        ++init_count;
        initialized_ = init_result;
        return init_result;
    }

    bool activate() override
    {
        ++activate_count;
        return initialized_ && activate_result;
    }

    void deactivate() override { ++deactivate_count; }

    void resize(int width, int height) override
    {
        ++resize_count;
        last_width = width;
        last_height = height;
    }

    void shutdown() override
    {
        ++shutdown_count;
        initialized_ = false;
    }

    bool initialized() const override { return initialized_; }
    bool enabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    bool init_result = true;
    bool activate_result = true;
    int init_count = 0;
    int activate_count = 0;
    int deactivate_count = 0;
    int resize_count = 0;
    int shutdown_count = 0;
    int last_width = 0;
    int last_height = 0;

protected:
    bool renderScene(const Ecs::World&, Renderer::Internal::FrameOutput&) override { return true; }
    bool compose(Renderer::Internal::FrameOutput&) override { return true; }
    void present(Renderer::Internal::FrameOutput&) override {}

private:
    bool initialized_ = false;
    bool enabled_ = true;
};

class EnvironmentCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/environment/state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity disabled = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(
            disabled, Renderer::EnvironmentComponent{.enabled = false});

        const Ecs::Entity active = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(active, Renderer::EnvironmentComponent{
            .enabled = true,
            .sky_color = {0.1f, 0.2f, 0.3f},
            .intensity = -2.0f,
            .rotation_degrees = 37.0f,
            .ambient_color = {0.3f, 0.4f, 0.5f},
            .ambient_intensity = -1.0f,
            .fog = Renderer::FogMode::None,
        });

        const Renderer::EnvironmentState state = Renderer::environmentState(world);
        if (!Testing::require(state.valid, "environment state is invalid", error)) return false;
        if (!Testing::require(
                Testing::near(state.sky_color.x, 0.1f) &&
                Testing::near(state.sky_color.y, 0.2f) &&
                Testing::near(state.sky_color.z, 0.3f),
                "environment sky color mismatch", error))
            return false;
        if (!Testing::require(
                Testing::near(state.intensity, 0.0f) &&
                Testing::near(state.ambient_intensity, 0.0f) &&
                Testing::near(state.rotation_degrees, 37.0f),
                "environment normalization mismatch", error))
            return false;

        const std::uint64_t signature = Renderer::environmentSignature(state);
        Renderer::EnvironmentState changed = state;
        changed.rotation_degrees += 1.0f;
        return Testing::require(
            signature != Renderer::environmentSignature(changed),
            "environment signature ignored changed state", error);
    }
};

class LinearFogCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/fog/linear"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(entity, Renderer::EnvironmentComponent{
            .fog = Renderer::FogMode::Linear,
            .fog_color = {0.2f, 0.4f, 0.6f},
            .fog_start = 12.0f,
            .fog_end = 40.0f,
        });
        const Renderer::EnvironmentState state = Renderer::environmentState(world);
        return Testing::require(state.fog == Renderer::FogMode::Linear, "linear fog mode lost", error) &&
            Testing::require(
                Testing::near(state.fog_start, 12.0f) && Testing::near(state.fog_end, 40.0f),
                "linear fog range mismatch", error);
    }
};

class ExponentialFogCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/fog/exponential"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(entity, Renderer::EnvironmentComponent{
            .fog = Renderer::FogMode::Exponential,
            .fog_density = -0.25f,
            .fog_start = 5.0f,
            .fog_end = 5.0f,
        });
        const Renderer::EnvironmentState state = Renderer::environmentState(world);
        return Testing::require(state.fog == Renderer::FogMode::Exponential, "exponential fog mode lost", error) &&
            Testing::require(Testing::near(state.fog_density, 0.0f), "fog density was not clamped", error) &&
            Testing::require(state.fog_end > state.fog_start, "fog end was not normalized", error);
    }
};

class GlobalIlluminationCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/global-illumination/state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = world.createEntity();
        const Renderer::GlobalIlluminationComponent expected{
            .enabled = true,
            .intensity = 1.5f,
            .bounces = 3u,
            .photon_mapping = true,
            .photon_count = 4096u,
            .photon_radius = 0.75f,
        };
        world.add<Renderer::GlobalIlluminationComponent>(entity, expected);
        const auto *stored = world.get<Renderer::GlobalIlluminationComponent>(entity);
        if (!Testing::require(stored != nullptr, "GI component missing", error)) return false;
        if (!Testing::require(
                stored->enabled && Testing::near(stored->intensity, 1.5f) &&
                stored->bounces == 3u && stored->photon_mapping &&
                stored->photon_count == 4096u && Testing::near(stored->photon_radius, 0.75f),
                "GI component state mismatch", error))
            return false;

        Renderer::GlobalIllumination::setPaused(true);
        const bool paused = Renderer::GlobalIllumination::paused();
        Renderer::GlobalIllumination::setPaused(false);
        Renderer::GlobalIllumination::reset();
        return Testing::require(paused && !Renderer::GlobalIllumination::paused(),
                                "GI pause state did not round-trip", error);
    }
};

class PhotonMapLifecycleCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/photon-mapping/lifecycle"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::GlobalIllumination::PhotonMapping::PhotonMap map;
        if (!Testing::require(!map.valid() && map.photonCount() == 0u,
                              "new photon map is unexpectedly populated", error))
            return false;
        map.clear();
        const Renderer::Vec3 sample = map.sample({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        return Testing::require(
            !map.valid() && map.photonCount() == 0u &&
            Testing::near(sample.x, 0.0f) && Testing::near(sample.y, 0.0f) &&
            Testing::near(sample.z, 0.0f),
            "cleared photon map returned residual state", error);
    }
};

class LodCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/lod/data"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::LodGroup group;
        group.levels = {
            Renderer::LodLevel{.minimum_distance = 0.0f, .mesh = 1u, .material = 2u},
            Renderer::LodLevel{.minimum_distance = 20.0f, .mesh = 3u, .material = 4u},
        };
        return Testing::require(group.levels.size() == 2u, "LOD levels lost", error) &&
            Testing::require(
                Testing::near(group.levels[1].minimum_distance, 20.0f) &&
                group.levels[1].mesh == 3u && group.levels[1].material == 4u,
                "LOD level data mismatch", error);
    }
};

class RendererManagerLifecycleCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/manager/lifecycle"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Manager manager;
        MockRenderer& first = manager.add<MockRenderer>("first");
        MockRenderer& second = manager.add<MockRenderer>("second");

        if (!Testing::require(manager.count() == 2u, "renderer manager count mismatch", error)) return false;
        if (!Testing::require(manager.initialize(), "renderer manager failed to initialize", error)) return false;
        if (!Testing::require(manager.active() == &first && first.activate_count == 1,
                              "renderer manager did not activate first renderer", error))
            return false;

        manager.resize(800, 450);
        if (!Testing::require(
                first.resize_count == 1 && second.resize_count == 1 &&
                first.last_width == 800 && second.last_height == 450,
                "renderer manager resize did not reach all available renderers", error))
            return false;

        if (!Testing::require(manager.activate("second") && manager.active() == &second,
                              "renderer manager named switch failed", error))
            return false;
        if (!Testing::require(manager.next() && manager.active() == &first,
                              "renderer manager next switch failed", error))
            return false;

        manager.shutdown();
        return Testing::require(
            first.shutdown_count == 1 && second.shutdown_count == 1 && manager.active() == nullptr,
            "renderer manager shutdown leaked active state", error);
    }
};

class DebugInspectorCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/debug/inspector-settings"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Debug::Inspector inspector;
        inspector.setShowBvh(true);
        inspector.setShowViewport(true);
        inspector.setBvhLevel(3);
        inspector.setOverlayOpacity(0.35f);
        return Testing::require(inspector.showBvh() && inspector.showViewport(),
                                "debug inspector visibility settings mismatch", error) &&
            Testing::require(inspector.bvhLevel() == 3, "debug inspector BVH level mismatch", error) &&
            Testing::require(Testing::near(inspector.overlayOpacity(), 0.35f),
                             "debug inspector opacity mismatch", error);
    }
};

} // namespace

void registerRenderingSystems(Testing::Runner& runner)
{
    runner.add<EnvironmentCase>();
    runner.add<LinearFogCase>();
    runner.add<ExponentialFogCase>();
    runner.add<GlobalIlluminationCase>();
    runner.add<PhotonMapLifecycleCase>();
    runner.add<LodCase>();
    runner.add<RendererManagerLifecycleCase>();
    runner.add<DebugInspectorCase>();
}

} // namespace Tests
