#include "Debugging.hpp"

#include <Camera/Camera.hpp>
#include <Renderer/Components.hpp>
#include <Renderer/Debug/Debug.hpp>
#include <Renderer/Environment.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace Debugging {
namespace {

constexpr ImVec4 heading_color{1.0f, 0.82f, 0.0f, 1.0f};
constexpr ImVec4 profiler_background{0.02f, 0.03f, 0.28f, 0.96f};
constexpr ImVec4 profiler_border{0.38f, 0.62f, 1.0f, 1.0f};
constexpr ImVec4 profiler_plot{1.0f, 0.9f, 0.0f, 1.0f};

void section(const char *label)
{
    ImGui::Spacing();
    ImGui::TextColored(heading_color, "%s", label);
    ImGui::Separator();
}

void markCamera(Ecs::World& world)
{
    world.markChanged(Ecs::ChangeKind::Camera);
}

void markLighting(Ecs::World& world)
{
    world.markChanged(Ecs::ChangeKind::Lighting);
}

void drawDebugMenu(State& state, Context& context)
{
    if (!ImGui::BeginMenu("Debug")) return;

    ImGui::MenuItem("Display FPS", nullptr, &state.show_fps);
    ImGui::MenuItem("Display position", nullptr, &state.show_position);

    ImGui::BeginDisabled();
    ImGui::MenuItem("Wireframe", nullptr, false);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Not exposed by the current Horse rasterizer");

    section("Experimental");

    Renderer::Debug::Inspector& inspector = Renderer::Debug::inspector();

    bool frozen = inspector.frozen();
    if (ImGui::MenuItem("Freeze Scene", nullptr, &frozen)) {
        if (frozen) inspector.freeze(context.world, context.width, context.height);
        else inspector.unfreeze(context.world);
    }

    bool show_bvh = inspector.showBvh();
    if (ImGui::MenuItem("Show BVH", nullptr, &show_bvh))
        inspector.setShowBvh(show_bvh);

    bool show_viewport = inspector.showViewport();
    if (ImGui::MenuItem("Show Viewport", nullptr, &show_viewport))
        inspector.setShowViewport(show_viewport);

    const Renderer::Debug::BvhInfo bvh = inspector.bvhInfo();
    int level = inspector.bvhLevel();
    const int maximum_level = std::max(bvh.maximum_level, 0);
    if (ImGui::SliderInt("BVH Level", &level, 0, maximum_level))
        inspector.setBvhLevel(level);

    float opacity = inspector.overlayOpacity();
    if (ImGui::SliderFloat("Overlay Opacity", &opacity, 0.0f, 1.0f, "%.2f"))
        inspector.setOverlayOpacity(opacity);

    ImGui::EndMenu();
}

void drawCameraMenu(Context& context)
{
    if (!ImGui::BeginMenu("Camera")) return;

    Camera::CameraComponent *camera = context.world.get<Camera::CameraComponent>(context.camera);
    if (camera) {
        if (ImGui::SliderFloat("FOV", &camera->fov_degrees, 20.0f, 140.0f, "%.1f deg"))
            markCamera(context.world);

        if (ImGui::DragFloat("Near Plane", &camera->near_plane, 0.005f, 0.0001f, 100.0f, "%.4f"))
            markCamera(context.world);

        if (ImGui::DragFloat("Far Plane", &camera->far_plane, 1.0f, 0.0f, 100000.0f, "%.1f"))
            markCamera(context.world);

        int projection = camera->projection == Camera::Projection::Perspective ? 0 : 1;
        const char *projections[] = {"Perspective", "Orthographic"};
        if (ImGui::Combo("Projection", &projection, projections, 2)) {
            camera->projection = projection == 0
                ? Camera::Projection::Perspective
                : Camera::Projection::Orthographic;
            markCamera(context.world);
        }

        if (camera->projection == Camera::Projection::Orthographic) {
            if (ImGui::DragFloat("Width", &camera->xmag, 0.05f, 0.01f, 10000.0f, "%.2f"))
                markCamera(context.world);
            if (ImGui::DragFloat("Height", &camera->ymag, 0.05f, 0.01f, 10000.0f, "%.2f"))
                markCamera(context.world);
        }
    }

    ImGui::Separator();

    float speed = context.camera_controller.speed();
    if (ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 10000.0f, "%.2f"))
        context.camera_controller.setSpeed(speed);

    float sprint = context.camera_controller.sprintMultiplier();
    if (ImGui::DragFloat("Sprint Multiplier", &sprint, 0.1f, 0.0f, 1000.0f, "%.2f"))
        context.camera_controller.setSprintMultiplier(sprint);

    float sensitivity = context.camera_controller.mouseSensitivity();
    if (ImGui::DragFloat("Mouse Sensitivity", &sensitivity, 0.005f, 0.0f, 10.0f, "%.3f"))
        context.camera_controller.setMouseSensitivity(sensitivity);

    float pitch[2] = {
        context.camera_controller.minimumPitch(),
        context.camera_controller.maximumPitch(),
    };
    if (ImGui::DragFloat2("Pitch Range", pitch, 0.1f, -179.0f, 179.0f, "%.1f"))
        context.camera_controller.setPitchRange(pitch[0], pitch[1]);

    ImGui::EndMenu();
}

void drawRendererMenu(Context& context)
{
    if (!ImGui::BeginMenu("Renderer")) return;

    bool viewport_culling = context.renderer.viewportCulling();
    if (ImGui::MenuItem("Viewport Culling", nullptr, &viewport_culling))
        context.renderer.setViewportCulling(viewport_culling);

    int shadow_resolution = context.renderer.shadowResolution();
    if (ImGui::SliderInt("Shadow Resolution", &shadow_resolution, 128, 4096))
        context.renderer.setShadowResolution(shadow_resolution);

    int shadow_cascades = context.renderer.shadowCascades();
    if (ImGui::SliderInt("Shadow Cascades", &shadow_cascades, 1, 8))
        context.renderer.setShadowCascades(shadow_cascades);

    float shadow_distance = context.renderer.shadowDistance();
    if (ImGui::DragFloat("Shadow Distance", &shadow_distance, 1.0f, 1.0f, 10000.0f, "%.1f"))
        context.renderer.setShadowDistance(shadow_distance);

    float shadow_near = context.renderer.shadowNearPlane();
    if (ImGui::DragFloat("Shadow Near Plane", &shadow_near, 0.005f, 0.0001f, 100.0f, "%.4f"))
        context.renderer.setShadowNearPlane(shadow_near);

    Renderer::Vec4 clear = context.renderer.clearColor();
    float clear_color[4] = {clear.x, clear.y, clear.z, clear.w};
    if (ImGui::ColorEdit4("Clear Color", clear_color)) {
        context.renderer.setClearColor({
            clear_color[0], clear_color[1], clear_color[2], clear_color[3]
        });
    }

    ImGui::EndMenu();
}

void drawEnvironmentMenu(Context& context)
{
    if (!ImGui::BeginMenu("Environment")) return;

    Renderer::EnvironmentComponent *environment =
        context.world.get<Renderer::EnvironmentComponent>(context.environment);

    if (!environment) {
        ImGui::TextDisabled("No environment component");
        ImGui::EndMenu();
        return;
    }

    if (ImGui::MenuItem("Enabled", nullptr, &environment->enabled))
        markLighting(context.world);

    if (ImGui::DragFloat("Intensity", &environment->intensity, 0.05f, 0.0f, 1000.0f, "%.2f"))
        markLighting(context.world);

    if (ImGui::DragFloat("Rotation", &environment->rotation_degrees, 0.5f, -360.0f, 360.0f, "%.1f deg"))
        markLighting(context.world);

    float sky[3] = {
        environment->sky_color.x,
        environment->sky_color.y,
        environment->sky_color.z,
    };
    if (ImGui::ColorEdit3("Sky Color", sky)) {
        environment->sky_color = {sky[0], sky[1], sky[2]};
        markLighting(context.world);
    }

    float ambient[3] = {
        environment->ambient_color.x,
        environment->ambient_color.y,
        environment->ambient_color.z,
    };
    if (ImGui::ColorEdit3("Ambient Color", ambient)) {
        environment->ambient_color = {ambient[0], ambient[1], ambient[2]};
        markLighting(context.world);
    }

    if (ImGui::DragFloat("Ambient Intensity", &environment->ambient_intensity, 0.01f, 0.0f, 1000.0f, "%.3f"))
        markLighting(context.world);

    section("Fog");

    int fog = static_cast<int>(environment->fog);
    const char *fog_modes[] = {"None", "Linear", "Exponential"};
    if (ImGui::Combo("Mode", &fog, fog_modes, 3)) {
        environment->fog = static_cast<Renderer::FogMode>(fog);
        markLighting(context.world);
    }

    if (environment->fog != Renderer::FogMode::None) {
        float fog_color[3] = {
            environment->fog_color.x,
            environment->fog_color.y,
            environment->fog_color.z,
        };
        if (ImGui::ColorEdit3("Fog Color", fog_color)) {
            environment->fog_color = {fog_color[0], fog_color[1], fog_color[2]};
            markLighting(context.world);
        }

        if (environment->fog == Renderer::FogMode::Exponential) {
            if (ImGui::DragFloat("Density", &environment->fog_density, 0.0001f, 0.0f, 100.0f, "%.5f"))
                markLighting(context.world);
        } else {
            if (ImGui::DragFloat("Start", &environment->fog_start, 0.5f, 0.0f, 100000.0f, "%.1f"))
                markLighting(context.world);
            if (ImGui::DragFloat("End", &environment->fog_end, 0.5f, 0.0f, 100000.0f, "%.1f"))
                markLighting(context.world);
        }
    }

    ImGui::EndMenu();
}

void drawLightMenu(Context& context)
{
    if (!ImGui::BeginMenu("Light")) return;

    Renderer::LightComponent *light = context.world.get<Renderer::LightComponent>(context.light);
    Renderer::Transform *transform = context.world.get<Renderer::Transform>(context.light);
    Renderer::ShadowComponent *shadow = context.world.get<Renderer::ShadowComponent>(context.light);

    if (!light || !transform) {
        ImGui::TextDisabled("No light component");
        ImGui::EndMenu();
        return;
    }

    int type = static_cast<int>(light->type);
    const char *types[] = {"Directional", "Point", "Spot"};
    if (ImGui::Combo("Type", &type, types, 3)) {
        light->type = static_cast<Renderer::LightType>(type);
        markLighting(context.world);
    }

    float color[3] = {light->color.x, light->color.y, light->color.z};
    if (ImGui::ColorEdit3("Color", color)) {
        light->color = {color[0], color[1], color[2]};
        markLighting(context.world);
    }

    if (ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 1000000.0f, "%.2f"))
        markLighting(context.world);

    if (light->type != Renderer::LightType::Directional &&
        ImGui::DragFloat("Range", &light->range, 0.5f, 0.0f, 100000.0f, "%.1f"))
        markLighting(context.world);

    if (light->type == Renderer::LightType::Spot) {
        if (ImGui::SliderFloat("Inner Cone", &light->inner_cone_degrees, 0.0f, 179.0f, "%.1f deg"))
            markLighting(context.world);
        if (ImGui::SliderFloat("Outer Cone", &light->outer_cone_degrees, 0.0f, 179.0f, "%.1f deg"))
            markLighting(context.world);
    }

    if (ImGui::DragFloat3("Position", &transform->position.x, 0.05f)) {
        context.world.markChanged(Ecs::ChangeKind::Transform);
        markLighting(context.world);
    }

    if (light->type != Renderer::LightType::Point &&
        ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.25f)) {
        context.world.markChanged(Ecs::ChangeKind::Transform);
        markLighting(context.world);
    }

    if (shadow) {
        section("Shadows");
        if (ImGui::MenuItem("Enabled##LightShadows", nullptr, &shadow->enabled))
            markLighting(context.world);
        if (ImGui::DragFloat("Bias", &shadow->bias, 0.0001f, 0.0f, 1.0f, "%.5f"))
            markLighting(context.world);
    }

    ImGui::EndMenu();
}

void drawTopBar(State& state, Context& context)
{
    if (!ImGui::BeginMainMenuBar()) return;

    drawDebugMenu(state, context);
    drawCameraMenu(context);
    drawRendererMenu(context);
    drawEnvironmentMenu(context);
    drawLightMenu(context);

    ImGui::EndMainMenuBar();
}

float graphMaximum(const State& state)
{
    const std::size_t count = state.frame_history_filled
        ? State::FRAME_HISTORY
        : state.frame_index;

    float maximum = 16.67f;
    for (std::size_t i = 0; i < count; ++i)
        maximum = std::max(maximum, state.frame_ms[i]);

    return maximum * 1.15f;
}

void metric(const char *label, float milliseconds)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::Text("%.2f ms", milliseconds);
}

void drawPerformance(State& state)
{
    if (!state.show_fps) return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - 8.0f, ImGui::GetFrameHeight() + 8.0f),
        ImGuiCond_FirstUseEver,
        ImVec2(1.0f, 0.0f)
    );
    ImGui::SetNextWindowSize(ImVec2(350.0f, 360.0f), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_Border, profiler_border);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, profiler_plot);

    if (ImGui::Begin(
            "Performance",
            &state.show_fps,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(1.65f);
        ImGui::Text("%.1f FPS", state.fps);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("%.2f ms", state.frame_ms_current);
        ImGui::Separator();

        const int count = static_cast<int>(state.frame_history_filled
            ? State::FRAME_HISTORY
            : state.frame_index);
        const int offset = state.frame_history_filled
            ? static_cast<int>(state.frame_index)
            : 0;

        if (count > 0) {
            char overlay[64]{};
            std::snprintf(overlay, sizeof(overlay), "Frame %.2f ms", state.frame_ms_current);
            ImGui::PlotHistogram(
                "##FrameHistory",
                state.frame_ms.data(),
                count,
                offset,
                overlay,
                0.0f,
                graphMaximum(state),
                ImVec2(0.0f, 92.0f)
            );
        }

        ImGui::Separator();
        ImGui::TextColored(heading_color, "%s", "MainThread");
        metric("Frame", state.frame_ms_current);
        metric("Update", state.update_ms);
        metric("Renderer", state.render_ms);
        metric("Debug UI", state.ui_ms);
    }
    ImGui::End();

    ImGui::PopStyleColor(5);
}

void drawPosition(State& state, Context& context)
{
    if (!state.show_position) return;

    const Renderer::Transform *transform =
        context.world.get<Renderer::Transform>(context.camera);
    if (!transform) return;

    ImGui::SetNextWindowSize(ImVec2(250.0f, 130.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Position", &state.show_position, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("X  %.3f", transform->position.x);
        ImGui::Text("Y  %.3f", transform->position.y);
        ImGui::Text("Z  %.3f", transform->position.z);
    }
    ImGui::End();
}

} // namespace

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.06f, 0.97f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.04f, 0.04f, 0.05f, 0.98f);
    colors[ImGuiCol_Header] = ImVec4(0.12f, 0.18f, 0.28f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.30f, 0.48f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.36f, 0.58f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.30f, 0.34f, 1.0f);
}

void sample(State& state, float delta_seconds)
{
    const float milliseconds = delta_seconds > 0.0f
        ? delta_seconds * 1000.0f
        : 0.0f;

    state.frame_ms_current = milliseconds;
    state.fps = delta_seconds > 0.0f ? 1.0f / delta_seconds : 0.0f;

    state.frame_ms[state.frame_index] = milliseconds;
    state.frame_index++;
    if (state.frame_index == State::FRAME_HISTORY) {
        state.frame_index = 0;
        state.frame_history_filled = true;
    }
}

void draw(State& state, Context& context)
{
    drawTopBar(state, context);
    drawPerformance(state);
    drawPosition(state, context);
}

} // namespace Debugging
