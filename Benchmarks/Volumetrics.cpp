#include <Window/Window.hpp>

#include <Camera/Camera.hpp>
#include <Ecs/Ecs.hpp>
#include <Models/Models.hpp>
#include <Renderer/Components.hpp>
#include <Renderer/Features.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Quality.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/SDLGPU/Context.hpp>
#include <Renderer/Volumetrics/Volumetrics.hpp>

#include <Rendering/VolumetricsBenchmark.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;
using Rendering::VolumetricsBenchmark::Samples;

constexpr std::size_t WarmupFrames = 20u;
constexpr std::size_t SampleFrames = 60u;

struct QualityCase {
    Renderer::Quality quality = Renderer::Quality::High;
    const char *name = "High";
};

constexpr std::array<QualityCase, 4> QualityCases {{
    {Renderer::Quality::Low, "Low"},
    {Renderer::Quality::Medium, "Medium"},
    {Renderer::Quality::High, "High"},
    {Renderer::Quality::Ultra, "Ultra"},
}};

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

void addVolumetricLight(Ecs::World& world)
{
    const Ecs::Entity light = world.createEntity();
    world.add<Renderer::Transform>(light, Renderer::Transform{
        .position = {0.0f, 0.0f, -10.0f},
    });
    world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
        .type = Renderer::LightType::Point,
        .color = {1.0f, 0.92f, 0.78f},
        .intensity = 700.0f,
        .range = 26.0f,
    });
    world.add<Renderer::VolumetricLightComponent>(
        light,
        Renderer::VolumetricLightComponent{
            .enabled = true,
            .intensity = 1.0f,
        }
    );
}

bool sampleMode(
    Renderer::Manager& renderers,
    Ecs::World& world,
    bool enabled,
    Samples& samples)
{
    Renderer::Features::settings().volumetrics = enabled;

    for (std::size_t frame = 0u; frame < WarmupFrames + SampleFrames; ++frame) {
        if (!Window::poll()) return false;

        const Clock::time_point started = Clock::now();
        renderers.render(world);
        if (!SDL_WaitForGPUIdle(Renderer::SDLGPU::device())) return false;
        const double milliseconds = std::chrono::duration<double, std::milli>(
            Clock::now() - started
        ).count();

        if (frame >= WarmupFrames) samples.add(milliseconds);
    }
    return true;
}

bool benchmarkQuality(
    Renderer::Manager& renderers,
    Ecs::World& world,
    const QualityCase& quality_case)
{
    Renderer::Volumetrics::setQuality(quality_case.quality);

    Samples disabled;
    Samples enabled;
    if (!sampleMode(renderers, world, false, disabled) ||
        !sampleMode(renderers, world, true, enabled) ||
        !sampleMode(renderers, world, true, enabled) ||
        !sampleMode(renderers, world, false, disabled))
        return false;

    const Rendering::VolumetricsBenchmark::Comparison comparison{
        disabled.averageMs(),
        enabled.averageMs(),
    };

    const Renderer::Volumetrics::Settings& settings =
        Renderer::Volumetrics::currentSettings();
    std::printf(
        "[Volumetrics Benchmark] %s: divisor=%d samples=%u blur=%u, %zu samples/mode\n",
        quality_case.name,
        settings.resolution_divisor,
        settings.sample_count,
        settings.blur_passes,
        disabled.count
    );
    std::printf(
        "[Volumetrics Benchmark]   disabled %.3f ms | enabled %.3f ms | overhead %+.3f ms (%+.2f%%)\n",
        comparison.disabled_ms,
        comparison.enabled_ms,
        comparison.deltaMs(),
        comparison.overheadPercent()
    );
    return true;
}

} // namespace

int main()
{
    Window::Settings window;
    window.title = "GAME Volumetrics Benchmark";
    window.width = 1280;
    window.height = 720;
    window.resizable = false;
    if (!Window::create(window)) {
        std::fprintf(stderr, "[Volumetrics Benchmark] window creation failed\n");
        return 1;
    }

    const std::filesystem::path scene_path =
        std::filesystem::temp_directory_path() / "game-volumetrics-benchmark.obj";
    if (!writeScene(scene_path)) {
        std::fprintf(stderr, "[Volumetrics Benchmark] could not create temporary scene\n");
        Window::destroy();
        return 1;
    }

    std::string error;
    const Models::ModelHandle model = Models::load(scene_path.string(), &error);
    if (model == Models::INVALID_MODEL) {
        std::fprintf(stderr, "[Volumetrics Benchmark] model load failed: %s\n", error.c_str());
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
        std::fprintf(stderr, "[Volumetrics Benchmark] scene instantiation failed: %s\n", error.c_str());
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }

    addVolumetricLight(world);

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
        std::fprintf(stderr, "[Volumetrics Benchmark] renderer initialization failed\n");
        Renderer::ModelScene::destroy(world, scene);
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }
    renderers.resize(Window::width(), Window::height());

    std::printf(
        "[Volumetrics Benchmark] %dx%d synchronized wall-time quality sweep\n",
        Window::width(),
        Window::height()
    );

    bool completed = true;
    for (const QualityCase& quality_case : QualityCases) {
        if (!benchmarkQuality(renderers, world, quality_case)) {
            completed = false;
            break;
        }
    }

    Renderer::Features::settings().volumetrics = false;
    renderers.shutdown();
    Renderer::ModelScene::destroy(world, scene);
    Models::clearCache();
    std::filesystem::remove(scene_path);
    Window::destroy();
    return completed ? 0 : 1;
}
