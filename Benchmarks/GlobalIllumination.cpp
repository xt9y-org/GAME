#include <Ecs/Ecs.hpp>
#include <Models/Models.hpp>
#include <Renderer/Components.hpp>
#include <Renderer/Features.hpp>
#include <Renderer/GlobalIllumination/Debug.hpp>
#include <Renderer/GlobalIllumination/GlobalIllumination.hpp>
#include <Renderer/Internal/AccelerationState.hpp>
#include <Renderer/Internal/ShadingState.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Quality.hpp>

#include <Rendering/Benchmark.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int SyntheticGrid = 64;
constexpr std::size_t TransformSamples = 64u;
constexpr std::size_t ScaleTransformSamples = 32u;
constexpr std::array<std::size_t, 4> InstanceSweep {{1u, 16u, 64u, 256u}};
constexpr const char *DefaultScene = "Assets/Sponza/sponza.obj";

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

void configureGiWorld(Ecs::World& world)
{
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

bool instantiateCopy(
    Ecs::World& world,
    Models::ModelHandle model,
    const Renderer::ModelScene::Options& base_options,
    std::size_t index,
    Renderer::ModelScene::Instance *scene,
    Ecs::Entity *parent,
    std::string *error)
{
    if (!scene || !parent) return false;

    *parent = world.createEntity();
    constexpr float spacing = 40.0f;
    constexpr std::size_t columns = 16u;
    Renderer::Transform transform;
    transform.position = {
        static_cast<float>(index % columns) * spacing,
        0.0f,
        static_cast<float>(index / columns) * spacing,
    };
    world.add<Renderer::Transform>(*parent, transform);

    Renderer::ModelScene::Options options = base_options;
    options.parent = *parent;
    if (Renderer::ModelScene::instantiate(world, model, scene, options, error)) return true;

    world.destroyEntity(*parent);
    *parent = Ecs::INVALID_ENTITY;
    return false;
}

void destroyCopies(
    Ecs::World& world,
    std::vector<Renderer::ModelScene::Instance>& scenes,
    const std::vector<Ecs::Entity>& parents)
{
    for (Renderer::ModelScene::Instance& scene : scenes)
        Renderer::ModelScene::destroy(world, scene);
    for (const Ecs::Entity parent : parents)
        if (parent != Ecs::INVALID_ENTITY && world.alive(parent)) world.destroyEntity(parent);
}

bool runInstanceSweep(
    Models::ModelHandle model,
    const Renderer::ModelScene::Options& scene_options)
{
    std::printf("[GI Benchmark] shared-BLAS instance sweep: 1 / 16 / 64 / 256 model copies\n");

    for (const std::size_t copy_count : InstanceSweep) {
        Renderer::GlobalIllumination::reset();
        Renderer::Internal::clearAccelerationState();

        Ecs::World world;
        std::vector<Renderer::ModelScene::Instance> scenes(copy_count);
        std::vector<Ecs::Entity> parents(copy_count, Ecs::INVALID_ENTITY);
        std::string error;

        bool instantiated = true;
        for (std::size_t index = 0u; index < copy_count; ++index) {
            if (instantiateCopy(
                    world,
                    model,
                    scene_options,
                    index,
                    &scenes[index],
                    &parents[index],
                    &error))
                continue;
            instantiated = false;
            break;
        }
        if (!instantiated) {
            std::fprintf(
                stderr,
                "[GI Benchmark] %zu-instance scene instantiation failed: %s\n",
                copy_count,
                error.c_str()
            );
            destroyCopies(world, scenes, parents);
            Renderer::Internal::clearAccelerationState();
            return false;
        }

        configureGiWorld(world);
        Renderer::Internal::updateShadingState(world);
        (void)Renderer::GlobalIllumination::update(world);
        const Renderer::GlobalIllumination::Debug::Statistics initial =
            Renderer::GlobalIllumination::Debug::statistics();

        Rendering::Benchmark::Samples tlas_samples;
        Rendering::Benchmark::Samples probe_samples;
        std::size_t geometry_updates = 0u;
        Renderer::Transform *transform = world.get<Renderer::Transform>(parents.front());
        for (std::size_t sample = 0u; sample < ScaleTransformSamples && transform; ++sample) {
            transform->position.x += (sample & 1u) == 0u ? 0.01f : -0.01f;
            world.markChanged(Ecs::ChangeKind::Transform);
            Renderer::Internal::updateShadingState(world);
            (void)Renderer::GlobalIllumination::update(world);

            const Renderer::GlobalIllumination::Debug::Statistics stats =
                Renderer::GlobalIllumination::Debug::statistics();
            if (stats.scene_update == Renderer::GlobalIllumination::Debug::SceneUpdate::Geometry) {
                tlas_samples.add(stats.scene_build_ms);
                ++geometry_updates;
            }
            probe_samples.add(stats.probe_update_ms);
        }

        std::printf(
            "[GI Benchmark] copies=%zu BLAS=%zu instances=%zu nodes=%zu depth=%u | build %.3f ms | TLAS %.3f ms median (%.3f mean) | probe %.3f ms median\n",
            copy_count,
            initial.blases,
            initial.instances,
            initial.bvh_nodes,
            initial.bvh_depth,
            initial.scene_build_ms,
            tlas_samples.medianMs(),
            tlas_samples.averageMs(),
            probe_samples.medianMs()
        );
        if (geometry_updates != ScaleTransformSamples) {
            std::printf(
                "[GI Benchmark]   warning: observed %zu/%zu transform-driven geometry updates\n",
                geometry_updates,
                ScaleTransformSamples
            );
        }

        Renderer::GlobalIllumination::reset();
        destroyCopies(world, scenes, parents);
        Renderer::Internal::clearAccelerationState();
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    bool generated_scene = false;
    std::filesystem::path scene_path;
    if (argc >= 2) {
        scene_path = argv[1];
    } else if (std::filesystem::exists(DefaultScene)) {
        scene_path = DefaultScene;
    } else {
        generated_scene = true;
        scene_path = std::filesystem::temp_directory_path() / "game-gi-benchmark.obj";
        if (!writeSyntheticScene(scene_path)) {
            std::fprintf(stderr, "[GI Benchmark] could not create synthetic scene\n");
            return 1;
        }
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

    configureGiWorld(world);

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
    Renderer::GlobalIllumination::reset();
    Renderer::Internal::clearAccelerationState();
    Renderer::Internal::updateShadingState(world);
    (void)Renderer::GlobalIllumination::update(world);

    const Renderer::GlobalIllumination::Debug::Statistics initial =
        Renderer::GlobalIllumination::Debug::statistics();

    std::printf(
        "[GI Benchmark] scene: %s\n",
        generated_scene ? "synthetic 64x64 grid" : scene_path.string().c_str()
    );
    std::printf(
        "[GI Benchmark] rigid acceleration: %zu BLAS, %zu instances, %zu unique triangles, %zu nodes, depth %u\n",
        initial.blases,
        initial.instances,
        initial.triangles,
        initial.bvh_nodes,
        initial.bvh_depth
    );
    std::printf(
        "[GI Benchmark] initial %s sync: %.3f ms CPU | first probe %.3f ms\n",
        sceneUpdateName(initial.scene_update),
        initial.scene_build_ms,
        initial.probe_update_ms
    );

    const Ecs::Entity movable = movableEntity(world, scene);
    if (movable == Ecs::INVALID_ENTITY) {
        std::fprintf(stderr, "[GI Benchmark] no movable render entity found\n");
        Renderer::GlobalIllumination::reset();
        Renderer::ModelScene::destroy(world, scene);
        Renderer::Internal::clearAccelerationState();
        Models::clearCache();
        if (generated_scene) std::filesystem::remove(scene_path);
        return 1;
    }

    Rendering::Benchmark::Samples tlas_samples;
    Rendering::Benchmark::Samples probe_samples;
    std::size_t geometry_updates = 0u;
    Renderer::Transform *transform = world.get<Renderer::Transform>(movable);
    for (std::size_t sample = 0u; sample < TransformSamples && transform; ++sample) {
        transform->position.x += (sample & 1u) == 0u ? 0.01f : -0.01f;
        world.markChanged(Ecs::ChangeKind::Transform);
        Renderer::Internal::updateShadingState(world);
        (void)Renderer::GlobalIllumination::update(world);

        const Renderer::GlobalIllumination::Debug::Statistics stats =
            Renderer::GlobalIllumination::Debug::statistics();
        if (stats.scene_update == Renderer::GlobalIllumination::Debug::SceneUpdate::Geometry) {
            tlas_samples.add(stats.scene_build_ms);
            ++geometry_updates;
        }
        probe_samples.add(stats.probe_update_ms);
    }

    const Renderer::GlobalIllumination::Debug::Statistics final_stats =
        Renderer::GlobalIllumination::Debug::statistics();
    std::printf(
        "[GI Benchmark] transform/TLAS sync: %.3f ms median (%.3f mean), %zu samples\n",
        tlas_samples.medianMs(),
        tlas_samples.averageMs(),
        geometry_updates
    );
    std::printf(
        "[GI Benchmark] probe update: %.3f ms median (%.3f mean), %zu samples\n",
        probe_samples.medianMs(),
        probe_samples.averageMs(),
        probe_samples.count
    );
    std::printf(
        "[GI Benchmark] updates topology=%llu geometry=%llu resources=%llu\n",
        static_cast<unsigned long long>(final_stats.topology_updates),
        static_cast<unsigned long long>(final_stats.geometry_updates),
        static_cast<unsigned long long>(final_stats.resource_updates)
    );

    if (generated_scene && !runInstanceSweep(model, scene_options)) {
        Renderer::GlobalIllumination::reset();
        Renderer::ModelScene::destroy(world, scene);
        Renderer::Internal::clearAccelerationState();
        Models::clearCache();
        std::filesystem::remove(scene_path);
        return 1;
    }

    Renderer::GlobalIllumination::reset();
    Renderer::ModelScene::destroy(world, scene);
    Renderer::Internal::clearAccelerationState();
    Models::clearCache();
    if (generated_scene) std::filesystem::remove(scene_path);
    return 0;
}
