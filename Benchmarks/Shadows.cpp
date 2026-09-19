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

#include <Rendering/Benchmark.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Rendering::Benchmark::Samples;

constexpr int Grid = 32;
constexpr int PointLights = 8;
constexpr int SpotLights = 8;
constexpr std::size_t WarmupFrames = 20u;
constexpr std::size_t SampleFrames = 80u;
constexpr std::size_t DirtyFrames = 40u;

bool writeScene(const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;

    constexpr float width = 34.0f;
    constexpr float height = 18.0f;
    for (int y = 0; y <= Grid; ++y) {
        const float fy = -height * 0.5f + height * static_cast<float>(y) /
            static_cast<float>(Grid);
        for (int x = 0; x <= Grid; ++x) {
            const float fx = -width * 0.5f + width * static_cast<float>(x) /
                static_cast<float>(Grid);
            const float z = -22.0f - 1.5f * static_cast<float>((x + y) & 1);
            file << "v " << fx << ' ' << fy << ' ' << z << '\n';
        }
    }
    file << "vn 0 0 1\n";

    const int stride = Grid + 1;
    for (int y = 0; y < Grid; ++y) {
        for (int x = 0; x < Grid; ++x) {
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

std::vector<Ecs::Entity> addLights(Ecs::World& world)
{
    std::vector<Ecs::Entity> lights;
    lights.reserve(PointLights + SpotLights);

    for (int index = 0; index < PointLights + SpotLights; ++index) {
        const bool spot = index >= PointLights;
        const int local = spot ? index - PointLights : index;
        const float x = -14.0f + 28.0f * static_cast<float>(local % 4) / 3.0f;
        const float y = -6.0f + 12.0f * static_cast<float>(local / 4);

        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .position = {x, y, -12.0f},
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type = spot ? Renderer::LightType::Spot : Renderer::LightType::Point,
            .color = {1.0f, 0.92f, 0.8f},
            .intensity = 900.0f,
            .range = 18.0f,
            .inner_cone_degrees = 24.0f,
            .outer_cone_degrees = 42.0f,
        });
        world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{});
        lights.push_back(light);
    }
    return lights;
}

double renderFrame(Renderer::Manager& renderers, Ecs::World& world)
{
    const Clock::time_point started = Clock::now();
    renderers.render(world);
    if (!SDL_WaitForGPUIdle(Renderer::SDLGPU::device())) return -1.0;
    return std::chrono::duration<double, std::milli>(Clock::now() - started).count();
}

bool sampleCached(Renderer::Manager& renderers, Ecs::World& world, Samples& samples)
{
    for (std::size_t frame = 0u; frame < WarmupFrames + SampleFrames; ++frame) {
        if (!Window::poll()) return false;
        const double milliseconds = renderFrame(renderers, world);
        if (milliseconds < 0.0) return false;
        if (frame >= WarmupFrames) samples.add(milliseconds);
    }
    return true;
}

bool sampleInvalidated(
    Renderer::Manager& renderers,
    Ecs::World& world,
    Ecs::Entity light,
    Samples& samples)
{
    Renderer::Transform *transform = world.get<Renderer::Transform>(light);
    if (!transform) return false;

    for (std::size_t frame = 0u; frame < DirtyFrames; ++frame) {
        if (!Window::poll()) return false;
        transform->position.x += (frame & 1u) == 0u ? 0.02f : -0.02f;
        world.markChanged(Ecs::ChangeKind::Transform);
        world.markChanged(Ecs::ChangeKind::Lighting);

        const double milliseconds = renderFrame(renderers, world);
        if (milliseconds < 0.0) return false;
        samples.add(milliseconds);
    }
    return true;
}

} // namespace

int main()
{
    Window::Settings window;
    window.title = "GAME Shadow Cache Benchmark";
    window.width = 1280;
    window.height = 720;
    window.resizable = false;
    if (!Window::create(window)) {
        std::fprintf(stderr, "[Shadow Benchmark] window creation failed\n");
        return 1;
    }

    const std::filesystem::path scene_path =
        std::filesystem::temp_directory_path() / "game-shadow-benchmark.obj";
    if (!writeScene(scene_path)) {
        std::fprintf(stderr, "[Shadow Benchmark] could not create temporary scene\n");
        Window::destroy();
        return 1;
    }

    std::string error;
    const Models::ModelHandle model = Models::load(scene_path.string(), &error);
    if (model == Models::INVALID_MODEL) {
        std::fprintf(stderr, "[Shadow Benchmark] model load failed: %s\n", error.c_str());
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
        std::fprintf(stderr, "[Shadow Benchmark] scene instantiation failed: %s\n", error.c_str());
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }

    const std::vector<Ecs::Entity> lights = addLights(world);

    Renderer::Features::Settings& features = Renderer::Features::settings();
    features = Renderer::Features::Settings{};
    features.shadows = true;
    features.environment = false;
    features.global_illumination = false;
    features.ambient_occlusion = false;
    features.reflections = false;
    features.volumetrics = false;
    features.gaussian_splat = false;

    Renderer::Manager renderers;
    Renderer::Rasterizer& rasterizer = renderers.add<Renderer::Rasterizer>("Rasterizer");
    rasterizer.setShadowQuality(Renderer::Quality::Medium);
    rasterizer.setDepthPrepass(false);
    rasterizer.setHiZ(false);
    rasterizer.setOcclusionCulling(false);
    rasterizer.setGpuDriven(false);
    rasterizer.setForwardPlus(false);

    if (!renderers.initialize()) {
        std::fprintf(stderr, "[Shadow Benchmark] renderer initialization failed\n");
        Renderer::ModelScene::destroy(world, scene);
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }
    renderers.resize(Window::width(), Window::height());

    if (!Window::poll()) {
        renderers.shutdown();
        Renderer::ModelScene::destroy(world, scene);
        Models::clearCache();
        std::filesystem::remove(scene_path);
        Window::destroy();
        return 1;
    }
    const double cold_ms = renderFrame(renderers, world);

    Samples cached;
    Samples invalidated;
    const bool completed = cold_ms >= 0.0 &&
        sampleCached(renderers, world, cached) &&
        !lights.empty() &&
        sampleInvalidated(renderers, world, lights.front(), invalidated);

    std::printf(
        "[Shadow Benchmark] %d point + %d spot shadow lights, %d triangles, Medium quality\n",
        PointLights,
        SpotLights,
        Grid * Grid * 2
    );
    std::printf("[Shadow Benchmark] cold dirty frame: %.3f ms synchronized wall\n", cold_ms);
    std::printf(
        "[Shadow Benchmark] cached frames: %.3f ms/frame synchronized wall (%zu samples)\n",
        cached.averageMs(),
        cached.count
    );
    std::printf(
        "[Shadow Benchmark] one-light invalidation: %.3f ms/frame synchronized wall (%zu samples)\n",
        invalidated.averageMs(),
        invalidated.count
    );
    std::printf(
        "[Shadow Benchmark] invalidation delta vs cached: %+.3f ms/frame\n",
        Rendering::Benchmark::deltaMs(cached.averageMs(), invalidated.averageMs())
    );

    renderers.shutdown();
    Renderer::ModelScene::destroy(world, scene);
    Models::clearCache();
    std::filesystem::remove(scene_path);
    Window::destroy();
    return completed ? 0 : 1;
}
