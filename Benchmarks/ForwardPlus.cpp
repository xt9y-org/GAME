#include <Window/Window.hpp>

#include <Camera/Camera.hpp>
#include <Ecs/Ecs.hpp>
#include <Models/Models.hpp>
#include <Renderer/Components.hpp>
#include <Renderer/Features.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Renderer.hpp>

#include <Rendering/ForwardPlusBenchmark.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;
using Rendering::ForwardPlusBenchmark::Samples;

constexpr std::size_t WarmupFrames = 30u;
constexpr std::size_t SampleFrames = 120u;
constexpr int LightGrid = 16;
constexpr int LightCount = LightGrid * LightGrid;
constexpr int PointLightCount = LightCount / 2;
constexpr int SpotLightCount = LightCount - PointLightCount;

bool writeScene(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;

    file <<
        "v -18 -10 -20\n"
        "v 18 -10 -20\n"
        "v 18 10 -20\n"
        "v -18 10 -20\n"
        "vn 0 0 1\n"
        "f 1//1 2//1 3//1\n"
        "f 1//1 3//1 4//1\n";
    return static_cast<bool>(file);
}

void addLights(Ecs::World& world)
{
    for (int y = 0; y < LightGrid; ++y) {
        for (int x = 0; x < LightGrid; ++x) {
            const float fx = -15.0f + 30.0f * static_cast<float>(x) /
                static_cast<float>(LightGrid - 1);
            const float fy = -8.0f + 16.0f * static_cast<float>(y) /
                static_cast<float>(LightGrid - 1);
            const bool spot = ((x + y) & 1) != 0;

            const Ecs::Entity light = world.createEntity();
            world.add<Renderer::Transform>(light, Renderer::Transform{
                .position = {fx, fy, -17.0f},
            });
            world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
                .type = spot ? Renderer::LightType::Spot : Renderer::LightType::Point,
                .color = {
                    0.65f + 0.35f * static_cast<float>(x & 1),
                    0.65f + 0.35f * static_cast<float>(y & 1),
                    1.0f,
                },
                .intensity = 180.0f,
                .range = 4.5f,
                .inner_cone_degrees = 24.0f,
                .outer_cone_degrees = 42.0f,
            });
        }
    }
}

bool sampleMode(
    Renderer::Manager& renderers,
    Renderer::Rasterizer& rasterizer,
    Ecs::World& world,
    bool forward_plus,
    Samples& samples)
{
    rasterizer.setForwardPlus(forward_plus);

    for (std::size_t frame = 0u; frame < WarmupFrames + SampleFrames; ++frame) {
        if (!Window::poll()) return false;

        const Clock::time_point started = Clock::now();
        renderers.render(world);
        if (!Renderer::waitIdle()) return false;
        const double milliseconds = std::chrono::duration<double, std::milli>(
            Clock::now() - started
        ).count();

        if (frame >= WarmupFrames) samples.add(milliseconds);
    }
    return true;
}

} // namespace

int main()
{
    Window::Settings window;
    window.title = "GAME Forward+ Benchmark";
    window.width = 1280;
    window.height = 720;
    window.resizable = false;
    if (!Window::create(window)) {
        std::fprintf(stderr, "[Forward+ Benchmark] window creation failed\n");
        return 1;
    }

    const std::filesystem::path scene_path =
        std::filesystem::temp_directory_path() / "game-forward-plus-benchmark.obj";
    if (!writeScene(scene_path)) {
        std::fprintf(stderr, "[Forward+ Benchmark] could not create temporary scene\n");
        Window::destroy();
        return 1;
    }

    std::string error;
    const Models::ModelHandle model = Models::load(scene_path.string(), &error);
    if (model == Models::INVALID_MODEL) {
        std::fprintf(stderr, "[Forward+ Benchmark] model load failed: %s\n", error.c_str());
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }

    Ecs::World world;
    const Ecs::Entity camera = world.createEntity();
    world.add<Renderer::Transform>(camera, Renderer::Transform{});
    world.add<Camera::CameraComponent>(camera, Camera::CameraComponent{
        .fov_degrees = 70.0f,
        .near_plane = 0.05f,
        .active = true,
        .far_plane = 100.0f,
    });

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
        std::fprintf(stderr, "[Forward+ Benchmark] scene instantiation failed: %s\n", error.c_str());
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }

    addLights(world);

    Renderer::Features::Settings& features = Renderer::Features::settings();
    features = Renderer::Features::Settings{};
    features.shadows = false;
    features.environment = false;
    features.global_illumination = false;
    features.ambient_occlusion = false;
    features.reflections = false;
    features.volumetrics = false;
    features.gaussian_splat = false;

    Renderer::Manager renderers;
    Renderer::Rasterizer& rasterizer = renderers.add<Renderer::Rasterizer>("Rasterizer");
    rasterizer.setDepthPrepass(false);
    rasterizer.setHiZ(false);
    rasterizer.setOcclusionCulling(false);
    rasterizer.setGpuDriven(false);
    rasterizer.setForwardPlus(false);

    if (!renderers.initialize()) {
        std::fprintf(stderr, "[Forward+ Benchmark] renderer initialization failed\n");
        Renderer::ModelScene::destroy(world, scene);
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }
    renderers.resize(Window::width(), Window::height());

    Samples full_loop;
    Samples forward_plus;
    const bool completed =
        sampleMode(renderers, rasterizer, world, false, full_loop) &&
        sampleMode(renderers, rasterizer, world, true, forward_plus) &&
        sampleMode(renderers, rasterizer, world, true, forward_plus) &&
        sampleMode(renderers, rasterizer, world, false, full_loop);

    const Rendering::ForwardPlusBenchmark::Comparison comparison{
        full_loop.medianMs(),
        forward_plus.medianMs(),
    };

    std::printf(
        "[Forward+ Benchmark] %d point + %d spot lights, %dx%d, %zu samples/mode\n",
        PointLightCount,
        SpotLightCount,
        Window::width(),
        Window::height(),
        full_loop.count
    );
    std::printf(
        "[Forward+ Benchmark] full light loop: %.3f ms median (%.3f mean) synchronized wall\n",
        comparison.full_loop_ms,
        full_loop.averageMs()
    );
    std::printf(
        "[Forward+ Benchmark] Forward+: %.3f ms median (%.3f mean) synchronized wall\n",
        comparison.forward_plus_ms,
        forward_plus.averageMs()
    );
    std::printf(
        "[Forward+ Benchmark] median delta: %+.3f ms/frame, speedup: %+.2f%%\n",
        comparison.deltaMs(),
        comparison.speedupPercent()
    );

    renderers.shutdown();
    Renderer::ModelScene::destroy(world, scene);
    Models::clearCache();
    std::filesystem::remove(scene_path);
    Window::destroy();
    return completed ? 0 : 1;
}
