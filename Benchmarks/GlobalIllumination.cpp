#include <Ecs/Ecs.hpp>
#include <Models/Models.hpp>
#include <Renderer/Components.hpp>
#include <Renderer/Features.hpp>
#include <Renderer/GlobalIllumination/Debug.hpp>
#include <Renderer/GlobalIllumination/GlobalIllumination.hpp>
#include <Renderer/Internal/ShadingState.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Quality.hpp>

#include <Rendering/Benchmark.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr int SyntheticGrid = 64;
constexpr std::size_t RefitSamples = 32u;

const char *sceneUpdateName(Renderer::GlobalIllumination::Debug::SceneUpdate update)
{
    using Renderer::GlobalIllumination::Debug::SceneUpdate;
    switch (update) {
    case SceneUpdate::None: return "none";
    case SceneUpdate::Resources: return "resources";
    case SceneUpdate::Geometry: return "geometry";
    case SceneUpdate::Topology: return "topology";
    }
    return "unknown";
}

bool writeSyntheticScene(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;

    constexpr float width = 32.0f;
    constexpr float height = 18.0f;
    for (int y = 0; y <= SyntheticGrid; ++y) {
        const float fy = -height * 0.5f + height * static_cast<float>(y) /
            static_cast<float>(SyntheticGrid);
        for (int x = 0; x <= SyntheticGrid; ++x) {
            const float fx = -width * 0.5f + width * static_cast<float>(x) /
                static_cast<float>(SyntheticGrid);
            file << "v " << fx << ' ' << fy << " -20\n";
        }
    }
    file << "vn 0 0 1\n";

    const int stride = SyntheticGrid + 1;
    for (int y = 0; y < SyntheticGrid; ++y) {
        for (int x = 0; x < SyntheticGrid; ++x) {
            const int a = y * stride + x + 1;
            const int b = a + 1;
            const int d = (y + 1) * stride + x + 1;
            const int c = d + 1;
            file << "f " << a << "//1 " << b << "//1 " << c << "//1\n";
            file << "f " << a << "//1 " << c << "//1 " << d << "//1\n";
        }
    }
    return static_cast<bool>(file);
}

Ecs::Entity movableEntity(
    Ecs::World& world,
    const Renderer::ModelScene::Instance& scene)
{
    for (const Renderer::ModelScene::NodeBinding& node : scene.nodes) {
        if (node.entity != Ecs::INVALID_ENTITY && world.get<Renderer::Transform>(node.entity))
            return node.entity;
        for (const Renderer::ModelScene::PartBinding& part : node.parts) {
            if (part.entity != Ecs::INVALID_ENTITY && world.get<Renderer::Transform>(part.entity))
                return part.entity;
        }
    }
    for (const Renderer::ModelScene::PartBinding& part : scene.loose_parts) {
        if (part.entity != Ecs::INVALID_ENTITY && world.get<Renderer::Transform>(part.entity))
            return part.entity;
    }
    return Ecs::INVALID_ENTITY;
}

} // namespace

int main(int argc, char **argv)
{
    bool generated_scene = argc < 2;
    std::filesystem::path scene_path;
    if (generated_scene) {
        scene_path = std::filesystem::temp_directory_path() / "game-gi-benchmark.obj";
        if (!writeSyntheticScene(scene_path)) {
            std::fprintf(stderr, "[GI Benchmark] could not create synthetic scene\n");
            return 1;
        }
    } else {
        scene_path = argv[1];
    }

    std::string error;
    const Models::ModelHandle model = Models::load(scene_path.string(), &error);
    if (model == Models::INVALID_MODEL) {
        std::fprintf(stderr, "[GI Benchmark] model load failed: %s\n", error.c_str());
        if (generated_scene) std::filesystem::remove(scene_path);
        return 1;
    }

    Ecs::World world;
    Renderer::ModelScene::Instance scene;
    Renderer::ModelScene::Options scene_options;
    scene_options.instantiate_cameras = false;
    scene_options.instantiate_lights = false;
    if (!Renderer::ModelScene::instantiate(
            world,
            model,
            &scene,
            scene_options,
            &error))
    {
        std::fprintf(stderr, "[GI Benchmark] scene instantiation failed: %s\n", error.c_str());
        Models::clearCache();
        if (generated_scene) std::filesystem::remove(scene_path);
        return 1;
    }

    const Ecs::Entity light = world.createEntity();
    world.add<Renderer::Transform>(light, Renderer::Transform{});
    world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
        .type = Renderer::LightType::Directional,
        .color = {1.0f, 1.0f, 1.0f},
        .intensity = 2.0f,
    });

    const Ecs::Entity gi = world.createEntity();
    world.add<Renderer::GlobalIlluminationComponent>(
        gi,
        Renderer::GlobalIlluminationComponent{
            .enabled = true,
            .intensity = 1.0f,
            .bounces = 1u,
            .photon_mapping = false,
        }
    );

    Renderer::Features::Settings& features = Renderer::Features::settings();
    features = Renderer::Features::Settings{};
    features.lighting = true;
    features.shadows = false;
    features.environment = false;
    features.global_illumination = true;
    features.ambient_occlusion = false;
    features.reflections = false;
    features.volumetrics = false;
    features.gaussian_splat = false;

    Renderer::GlobalIllumination::setQuality(Renderer::Quality::Low);
    Renderer::Internal::updateShadingState(world);
    (void)Renderer::GlobalIllumination::update(world);

    const Renderer::GlobalIllumination::Debug::Statistics initial =
        Renderer::GlobalIllumination::Debug::statistics();
    std::printf(
        "[GI Benchmark] scene: %s\n",
        generated_scene ? "synthetic 64x64 grid" : scene_path.string().c_str()
    );
    std::printf(
        "[GI Benchmark] triangles: %zu, BVH nodes: %zu, depth: %u\n",
        initial.triangles,
        initial.bvh_nodes,
        initial.bvh_depth
    );
    std::printf(
        "[GI Benchmark] initial %s sync: %.3f ms CPU\n",
        sceneUpdateName(initial.scene_update),
        initial.scene_build_ms
    );
    std::printf(
        "[GI Benchmark] first probe update: %.3f ms CPU\n",
        initial.probe_update_ms
    );

    Rendering::Benchmark::Samples refit_samples;
    Rendering::Benchmark::Samples probe_samples;
    std::size_t geometry_updates = 0u;
    const Ecs::Entity movable = movableEntity(world, scene);
    if (movable != Ecs::INVALID_ENTITY) {
        Renderer::Transform *transform = world.get<Renderer::Transform>(movable);
        for (std::size_t sample = 0u; sample < RefitSamples; ++sample) {
            if (!transform) break;
            transform->position.x += (sample & 1u) == 0u ? 0.01f : -0.01f;
            world.markChanged(Ecs::ChangeKind::Transform);
            Renderer::Internal::updateShadingState(world);
            (void)Renderer::GlobalIllumination::update(world);

            const Renderer::GlobalIllumination::Debug::Statistics stats =
                Renderer::GlobalIllumination::Debug::statistics();
            if (stats.scene_update == Renderer::GlobalIllumination::Debug::SceneUpdate::Geometry) {
                refit_samples.add(stats.scene_build_ms);
                ++geometry_updates;
            }
            probe_samples.add(stats.probe_update_ms);
        }
    }

    std::printf(
        "[GI Benchmark] geometry/refit sync: %.3f ms median (%.3f mean) CPU, %zu samples\n",
        refit_samples.medianMs(),
        refit_samples.averageMs(),
        geometry_updates
    );
    std::printf(
        "[GI Benchmark] probe update: %.3f ms median (%.3f mean) CPU, %zu samples\n",
        probe_samples.medianMs(),
        probe_samples.averageMs(),
        probe_samples.count
    );

    const Renderer::GlobalIllumination::Debug::Statistics final_stats =
        Renderer::GlobalIllumination::Debug::statistics();
    std::printf(
        "[GI Benchmark] cache updates topology=%llu geometry=%llu resources=%llu\n",
        static_cast<unsigned long long>(final_stats.topology_updates),
        static_cast<unsigned long long>(final_stats.geometry_updates),
        static_cast<unsigned long long>(final_stats.resource_updates)
    );

    Renderer::GlobalIllumination::reset();
    Renderer::ModelScene::destroy(world, scene);
    Models::clearCache();
    if (generated_scene) std::filesystem::remove(scene_path);
    return 0;
}
