#include "Font.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>

namespace {

using Clock = std::chrono::steady_clock;

const char *environment(const char *name)
{
    const char *value = std::getenv(name);
    return value && *value ? value : nullptr;
}

std::uint64_t frameLimit()
{
    const char *value = environment("RENDERCHECK_FRAME_LIMIT");
    if (!value) return environment("RENDERCHECK") ? 120u : 0u;

    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    return end && *end == '\0' ? static_cast<std::uint64_t>(parsed) : 0u;
}

void metric(const char *name, double value)
{
    const char *path = environment("RENDERCHECK_METRICS_PATH");
    if (!path) return;

    FILE *file = std::fopen(path, "a");
    if (!file) return;
    std::fprintf(file, "%s=%.6f\n", name, value);
    std::fclose(file);
}

Models::MeshHandle makeTriangle()
{
    Models::MeshData mesh;
    mesh.vertices = {
        Models::Vertex{.position = {-1.6f, -1.0f, 0.0f}},
        Models::Vertex{.position = {1.6f, -1.0f, 0.0f}},
        Models::Vertex{.position = {0.0f, 1.2f, 0.0f}},
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds.minimum = {-1.6f, -1.0f, 0.0f};
    mesh.bounds.maximum = {1.6f, 1.2f, 0.0f};
    return Models::registerMesh(std::move(mesh));
}

Models::MaterialHandle makeMaterial()
{
    Models::MaterialData material;
    material.name = "RendererCheck";
    material.color = {0.18f, 0.55f, 0.95f};
    return Models::registerMaterial(std::move(material));
}

struct Scene {
    Ecs::Entity triangle = Ecs::INVALID_ENTITY;
    Ecs::Entity fps = Ecs::INVALID_ENTITY;
    Ecs::Entity backend = Ecs::INVALID_ENTITY;
};

Scene makeScene(Ecs::World& world)
{
    const Ecs::Entity camera = world.createEntity();
    world.add<Renderer::Transform>(camera, Renderer::Transform{
        .position = {0.0f, 0.0f, 6.0f},
    });
    world.add<Camera::CameraComponent>(camera, Camera::CameraComponent{
        60.0f, 0.1f, true
    });

    Scene scene;
    scene.triangle = world.createEntity();
    world.add<Renderer::Transform>(scene.triangle, Renderer::Transform{});
    world.add<Renderer::MeshComponent>(scene.triangle, Renderer::MeshComponent{
        makeTriangle(), makeMaterial()
    });
    world.add<Renderer::RenderableComponent>(scene.triangle, Renderer::RenderableComponent{true});

    const Ecs::Entity light = world.createEntity();
    world.add<Renderer::Transform>(light, Renderer::Transform{
        .position = {0.0f, 2.5f, 4.0f},
    });
    world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
        .type = Renderer::LightType::Point,
        .color = {1.0f, 1.0f, 1.0f},
        .intensity = 24.0f,
    });

    Font::configureAtlas("", 16u, 16u, 8.0f);
    scene.fps = Font::screen(world, "FPS: 0", {12.0f, 12.0f}, 2.0f);
    scene.backend = Font::world(
        world,
        "Renderer: Rasterizer",
        Renderer::Transform{.position = {-2.3f, 1.8f, 0.0f}},
        0.28f,
        {1.0f, 1.0f, 1.0f, 1.0f},
        false
    );

    world.markChanged();
    return scene;
}

void updateText(
    Ecs::World& world,
    const Scene& scene,
    const Renderer::Manager& renderers,
    float fps)
{
    const Renderer::Manager::Entry *active = renderers.activeEntry();
    const char *backend = active ? active->name.c_str() : "None";
    char buffer[160]{};

    if (Font::TextComponent *text = world.get<Font::TextComponent>(scene.fps)) {
        std::snprintf(buffer, sizeof(buffer), "FPS: %.1f", fps);
        text->text = buffer;
    }

    if (Font::TextComponent *text = world.get<Font::TextComponent>(scene.backend))
        text->text = std::string("Renderer: ") + backend;
}

void setMouseLocked(bool locked)
{
    auto *window = static_cast<GLFWwindow *>(Display.getNativeWindow());
    if (!window) return;
    glfwSetInputMode(window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    Mouse.getDX();
    Mouse.getDY();
}

void updateCamera(Ecs::World& world, float delta, bool mouse_locked)
{
    const int mouse_dx = Mouse.getDX();
    const int mouse_dy = Mouse.getDY();
    if (!mouse_locked) return;

    const Ecs::Entity camera = Camera::activeCamera(world);
    if (camera == Ecs::INVALID_ENTITY) return;
    Renderer::Transform *transform = world.get<Renderer::Transform>(camera);
    if (!transform) return;

    bool changed = false;
    if (mouse_dx != 0 || mouse_dy != 0) {
        transform->rotation.y -= static_cast<float>(mouse_dx) * 0.12f;
        transform->rotation.x += static_cast<float>(mouse_dy) * 0.12f;
        transform->rotation.x = std::clamp(transform->rotation.x, -89.0f, 89.0f);
        changed = true;
    }

    const Renderer::Vec3 forward = Camera::flightDirection(
        transform->rotation.y,
        transform->rotation.x
    );
    const Renderer::Vec3 right = Camera::strafeDirection(transform->rotation.y);
    float speed = 3.0f * std::max(delta, 0.0f);
    if (Keyboard.isKeyDown(Keyboard.KEY_LSHIFT)) speed *= 3.0f;

    auto move = [&](const Renderer::Vec3& direction, float amount) {
        if (amount == 0.0f) return;
        transform->position.x += direction.x * amount;
        transform->position.y += direction.y * amount;
        transform->position.z += direction.z * amount;
        changed = true;
    };

    if (Keyboard.isKeyDown(Keyboard.KEY_W)) move(forward, speed);
    if (Keyboard.isKeyDown(Keyboard.KEY_S)) move(forward, -speed);
    if (Keyboard.isKeyDown(Keyboard.KEY_D)) move(right, speed);
    if (Keyboard.isKeyDown(Keyboard.KEY_A)) move(right, -speed);

    if (changed) world.markChanged();
}

void debugMenu(Renderer::Manager& renderers, float fps, bool mouse_locked)
{
    const bool visible = UI::beginFrame();
    if (!visible) return;

    const Renderer::Manager::Entry *active = renderers.activeEntry();
    const char *backend = active ? active->name.c_str() : "None";

    ImGui::SetNextWindowPos(ImVec2(12.0f, 48.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(230.0f, 0.0f), ImGuiCond_FirstUseEver);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Debug", nullptr, flags);
    if (ImGui::BeginCombo("Renderer", backend)) {
        for (std::size_t index = 0u; index < renderers.count(); ++index) {
            const Renderer::Manager::Entry *entry = renderers.entry(index);
            if (!entry) continue;

            const bool selected = index == renderers.activeIndex();
            if (!entry->available) {
                ImGui::TextDisabled("%s", entry->name.c_str());
                continue;
            }

            if (ImGui::Selectable(entry->name.c_str(), selected))
                renderers.activate(index);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    active = renderers.activeEntry();
    backend = active ? active->name.c_str() : "None";
    ImGui::Text("Renderer: %s", backend);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Mouse: %s", mouse_locked ? "captured" : "free");
    ImGui::TextUnformatted("TAB: toggle mouse");
    ImGui::End();
}

} // namespace

int main()
{
    lwcglInstallFastRuntime();
#ifdef __APPLE__
    lwcglSetContextVersion(2, 1);
    lwcglSetContextProfile(LWCGL_CONTEXT_ANY_PROFILE);
#else
    lwcglSetContextVersion(4, 3);
    lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

    Display.setDisplayMode(new DisplayMode(1280, 720));
    Display.create();
    Display.setTitle("RendererCheck");
    Keyboard.create();
    Mouse.create();
    bool mouse_locked = true;
    setMouseLocked(mouse_locked);

    Ecs::World world;
    const Scene scene = makeScene(world);

    Renderer::Manager renderers;
    auto& rasterizer = renderers.add<Renderer::Rasterizer>("Rasterizer");
    rasterizer.setEnabled(true);
    rasterizer.setClearColor({0.025f, 0.025f, 0.035f, 1.0f});

    auto& ray_tracer = renderers.add<Renderer::RayTracer>("Ray Tracer");
    ray_tracer.setEnabled(true);
    ray_tracer.setResolutionDivisor(4);
    ray_tracer.setExposure(1.05f);

    auto& path_tracer = renderers.add<Renderer::PathTracer>("Path Tracer");
    path_tracer.setEnabled(true);
    path_tracer.setResolutionDivisor(2);
    path_tracer.setSamplesPerFrame(1);
    path_tracer.setExposure(1.05f);
    path_tracer.setStationaryPhaseGrid(2);
    path_tracer.setResetPhaseGrid(1);
    path_tracer.setMovingPhaseGrid(4);
    path_tracer.setMovingDepthBlock(4);

    if (!renderers.initialize()) {
        std::fprintf(stderr, "Renderer initialization failed\n");
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 1;
    }

    int width = std::max(Display.getWidth(), 1);
    int height = std::max(Display.getHeight(), 1);
    renderers.resize(width, height);

    const std::uint64_t frame_limit = frameLimit();
    bool tab_down = false;
    std::uint64_t frame = 0u;
    auto previous = Clock::now();

    while (!Display.isCloseRequested()) {
        const auto frame_started = Clock::now();
        Display.processMessages();
        if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

        const bool tab = Keyboard.isKeyDown(Keyboard.KEY_TAB);
        if (tab && !tab_down) {
            mouse_locked = !mouse_locked;
            setMouseLocked(mouse_locked);
        }
        tab_down = tab;

        const auto now = Clock::now();
        const float delta = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f);
        previous = now;
        const float fps = delta > 0.0f ? 1.0f / delta : 0.0f;

        updateCamera(world, delta, mouse_locked);

        if (Renderer::Transform *transform = world.get<Renderer::Transform>(scene.triangle)) {
            transform->rotation.z += delta * 20.0f;
            world.markChanged();
        }

        const int next_width = std::max(Display.getWidth(), 1);
        const int next_height = std::max(Display.getHeight(), 1);
        if (next_width != width || next_height != height) {
            width = next_width;
            height = next_height;
            renderers.resize(width, height);
        }

        updateText(world, scene, renderers, fps);
        debugMenu(renderers, fps, mouse_locked);
        renderers.render(world);

        const double frame_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - frame_started
        ).count();
        metric("frame_ms", frame_ms);

        ++frame;
        if (frame_limit > 0u && frame >= frame_limit) break;
    }

    UI::shutdown();
    renderers.shutdown();
    Models::clearCache();
    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return 0;
}
