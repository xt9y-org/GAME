#include "Debugging.hpp"
#include "Layout.hpp"
#include "Values.hpp"

#include "../Loadout/Loadout.hpp"

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

void scalarWidth()
{
    ImGui::SetNextItemWidth(Layout::ControlWidth);
}

bool barFloat(
    const char *label,
    float *value,
    const Values::Range<float>& range,
    const char *format)
{
    ImGui::PushID(label);
    const float button_width = ImGui::GetFrameHeight();

    scalarWidth();
    bool changed = ImGui::SliderFloat(
        "##Value",
        value,
        range.minimum,
        range.maximum,
        format,
        ImGuiSliderFlags_AlwaysClamp
    );

    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(button_width, 0.0f))) {
        *value = Values::stepValue(
            *value, range.step, range.minimum, range.maximum, -1);
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(button_width, 0.0f))) {
        *value = Values::stepValue(
            *value, range.step, range.minimum, range.maximum, 1);
        changed = true;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool barInt(
    const char *label,
    int *value,
    const Values::Range<int>& range,
    const char *format = "%d")
{
    ImGui::PushID(label);
    const float button_width = ImGui::GetFrameHeight();

    scalarWidth();
    bool changed = ImGui::SliderInt(
        "##Value",
        value,
        range.minimum,
        range.maximum,
        format,
        ImGuiSliderFlags_AlwaysClamp
    );

    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(button_width, 0.0f))) {
        *value = Values::stepValue(
            *value, range.step, range.minimum, range.maximum, -1);
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(button_width, 0.0f))) {
        *value = Values::stepValue(
            *value, range.step, range.minimum, range.maximum, 1);
        changed = true;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

bool shadowResolutionBar(int *value)
{
    ImGui::PushID("Shadow Resolution");
    const float button_width = ImGui::GetFrameHeight();
    int slider = *value;

    scalarWidth();
    bool changed = ImGui::SliderInt(
        "##Value",
        &slider,
        Values::ShadowResolutions.front(),
        Values::ShadowResolutions.back(),
        "%d",
        ImGuiSliderFlags_AlwaysClamp
    );
    if (changed)
        slider = Values::ShadowResolutions[Values::shadowResolutionIndex(slider)];

    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(button_width, 0.0f))) {
        slider = Values::stepShadowResolution(slider, -1);
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(button_width, 0.0f))) {
        slider = Values::stepShadowResolution(slider, 1);
        changed = true;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Shadow Resolution");
    ImGui::PopID();

    if (changed) *value = slider;
    return changed;
}

void pushProfilerStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_Border, profiler_border);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, profiler_plot);
}

void popProfilerStyle()
{
    ImGui::PopStyleColor(5);
}

void drawLoadoutMenu(Context& context)
{
    if (!ImGui::BeginMenu("Loadout")) return;

    if (ImGui::BeginMenu("Weapon")) {
        for (std::size_t index = 0u; index < context.loadout.weapons.size(); ++index) {
            const Loadout::Item& item = context.loadout.weapons[index];
            ImGui::PushID(item.path.string().c_str());
            if (ImGui::MenuItem(
                    item.name.c_str(),
                    nullptr,
                    index == context.loadout.weapon))
                Loadout::selectWeapon(context.loadout, context.world, index);
            ImGui::PopID();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Arms")) {
        for (std::size_t index = 0u; index < context.loadout.arms.size(); ++index) {
            const Loadout::Item& item = context.loadout.arms[index];
            ImGui::PushID(item.path.string().c_str());
            if (ImGui::MenuItem(
                    item.name.c_str(),
                    nullptr,
                    index == context.loadout.arm))
                Loadout::selectArms(context.loadout, context.world, index);
            ImGui::PopID();
        }
        ImGui::EndMenu();
    }

    if (!context.loadout.error.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("%s", context.loadout.error.c_str());
    }

    ImGui::EndMenu();
}

void drawDebugMenu(State& state, Context& context)
{
    if (!ImGui::BeginMenu("Debug")) return;

    ImGui::MenuItem("Display FPS", nullptr, &state.show_fps);
    ImGui::MenuItem("Display Camera", nullptr, &state.show_camera);

    drawLoadoutMenu(context);

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
    if (barInt("BVH Level", &level, {0, maximum_level, 1}))
        inspector.setBvhLevel(level);

    float opacity = inspector.overlayOpacity();
    if (barFloat("Overlay Opacity", &opacity, Values::OverlayOpacity, "%.2f"))
        inspector.setOverlayOpacity(opacity);

    ImGui::EndMenu();
}

void drawCameraMenu(Context& context)
{
    if (!ImGui::BeginMenu("Camera")) return;

    Camera::CameraComponent *camera = context.world.get<Camera::CameraComponent>(context.camera);
    if (camera) {
        if (barFloat("FOV", &camera->fov_degrees, Values::CameraFov, "%.0f deg"))
            markCamera(context.world);

        if (barFloat("Near Plane", &camera->near_plane, Values::CameraNear, "%.2f"))
            markCamera(context.world);

        if (barFloat("Far Plane", &camera->far_plane, Values::CameraFar, "%.0f"))
            markCamera(context.world);

        int projection = camera->projection == Camera::Projection::Perspective ? 0 : 1;
        const char *projections[] = {"Perspective", "Orthographic"};
        scalarWidth();
        if (ImGui::Combo("Projection", &projection, projections, 2)) {
            camera->projection = projection == 0
                ? Camera::Projection::Perspective
                : Camera::Projection::Orthographic;
            markCamera(context.world);
        }

        if (camera->projection == Camera::Projection::Orthographic) {
            if (barFloat("Width", &camera->xmag, Values::OrthographicSize, "%.1f"))
                markCamera(context.world);
            if (barFloat("Height", &camera->ymag, Values::OrthographicSize, "%.1f"))
                markCamera(context.world);
        }
    }

    ImGui::Separator();

    float speed = context.camera_controller.speed();
    if (barFloat("Speed", &speed, Values::CameraSpeed, "%.0f"))
        context.camera_controller.setSpeed(speed);

    float sprint = context.camera_controller.sprintMultiplier();
    if (barFloat("Sprint Multiplier", &sprint, Values::SprintMultiplier, "%.0f"))
        context.camera_controller.setSprintMultiplier(sprint);

    float sensitivity = context.camera_controller.mouseSensitivity();
    if (barFloat("Mouse Sensitivity", &sensitivity, Values::MouseSensitivity, "%.2f"))
        context.camera_controller.setMouseSensitivity(sensitivity);

    float pitch[2] = {
        context.camera_controller.minimumPitch(),
        context.camera_controller.maximumPitch(),
    };
    scalarWidth();
    if (ImGui::DragFloat2("Pitch Range", pitch, 1.0f, -89.0f, 89.0f, "%.0f"))
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
    if (shadowResolutionBar(&shadow_resolution))
        context.renderer.setShadowResolution(shadow_resolution);

    int shadow_cascades = context.renderer.shadowCascades();
    if (barInt("Shadow Cascades", &shadow_cascades, Values::ShadowCascades))
        context.renderer.setShadowCascades(shadow_cascades);

    float shadow_distance = context.renderer.shadowDistance();
    if (barFloat("Shadow Distance", &shadow_distance, Values::ShadowDistance, "%.0f"))
        context.renderer.setShadowDistance(shadow_distance);

    float shadow_near = context.renderer.shadowNearPlane();
    if (barFloat("Shadow Near Plane", &shadow_near, Values::ShadowNear, "%.2f"))
        context.renderer.setShadowNearPlane(shadow_near);

    Renderer::Vec4 clear = context.renderer.clearColor();
    float clear_color[4] = {clear.x, clear.y, clear.z, clear.w};
    scalarWidth();
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

    if (barFloat("Intensity", &environment->intensity, Values::EnvironmentIntensity, "%.1f"))
        markLighting(context.world);

    if (barFloat("Rotation", &environment->rotation_degrees, Values::EnvironmentRotation, "%.0f deg"))
        markLighting(context.world);

    float sky[3] = {
        environment->sky_color.x,
        environment->sky_color.y,
        environment->sky_color.z,
    };
    scalarWidth();
    if (ImGui::ColorEdit3("Sky Color", sky)) {
        environment->sky_color = {sky[0], sky[1], sky[2]};
        markLighting(context.world);
    }

    float ambient[3] = {
        environment->ambient_color.x,
        environment->ambient_color.y,
        environment->ambient_color.z,
    };
    scalarWidth();
    if (ImGui::ColorEdit3("Ambient Color", ambient)) {
        environment->ambient_color = {ambient[0], ambient[1], ambient[2]};
        markLighting(context.world);
    }

    if (barFloat("Ambient Intensity", &environment->ambient_intensity, Values::AmbientIntensity, "%.2f"))
        markLighting(context.world);

    section("Fog");

    int fog = static_cast<int>(environment->fog);
    const char *fog_modes[] = {"None", "Linear", "Exponential"};
    scalarWidth();
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
        scalarWidth();
        if (ImGui::ColorEdit3("Fog Color", fog_color)) {
            environment->fog_color = {fog_color[0], fog_color[1], fog_color[2]};
            markLighting(context.world);
        }

        if (environment->fog == Renderer::FogMode::Exponential) {
            if (barFloat("Density", &environment->fog_density, Values::FogDensity, "%.3f"))
                markLighting(context.world);
        } else {
            if (barFloat("Start", &environment->fog_start, Values::FogDistance, "%.0f"))
                markLighting(context.world);
            if (barFloat("End", &environment->fog_end, Values::FogDistance, "%.0f"))
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
    scalarWidth();
    if (ImGui::Combo("Type", &type, types, 3)) {
        light->type = static_cast<Renderer::LightType>(type);
        markLighting(context.world);
    }

    float color[3] = {light->color.x, light->color.y, light->color.z};
    scalarWidth();
    if (ImGui::ColorEdit3("Color", color)) {
        light->color = {color[0], color[1], color[2]};
        markLighting(context.world);
    }

    const Values::Range<float>& intensity_range =
        light->type == Renderer::LightType::Directional
            ? Values::DirectionalIntensity
            : Values::LocalLightIntensity;
    const char *intensity_format =
        light->type == Renderer::LightType::Directional ? "%.1f" : "%.0f";
    if (barFloat("Intensity", &light->intensity, intensity_range, intensity_format))
        markLighting(context.world);

    if (light->type != Renderer::LightType::Directional &&
        barFloat("Range", &light->range, Values::LocalLightRange, "%.0f"))
        markLighting(context.world);

    if (light->type == Renderer::LightType::Spot) {
        if (barFloat("Inner Cone", &light->inner_cone_degrees, Values::ConeAngle, "%.0f deg"))
            markLighting(context.world);
        if (barFloat("Outer Cone", &light->outer_cone_degrees, Values::ConeAngle, "%.0f deg"))
            markLighting(context.world);
    }

    scalarWidth();
    if (ImGui::DragFloat3("Position", &transform->position.x, 1.0f, -10000.0f, 10000.0f, "%.0f")) {
        context.world.markChanged(Ecs::ChangeKind::Transform);
        markLighting(context.world);
    }

    if (light->type != Renderer::LightType::Point) {
        scalarWidth();
        if (ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f, -180.0f, 180.0f, "%.0f")) {
            context.world.markChanged(Ecs::ChangeKind::Transform);
            markLighting(context.world);
        }
    }

    if (shadow) {
        section("Shadows");
        if (ImGui::MenuItem("Enabled##LightShadows", nullptr, &shadow->enabled))
            markLighting(context.world);
        if (barFloat("Bias", &shadow->bias, Values::ShadowBias, "%.4f"))
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
        ImVec2(io.DisplaySize.x - Layout::PanelPadding, ImGui::GetFrameHeight() + Layout::PanelPadding),
        ImGuiCond_Always,
        ImVec2(1.0f, 0.0f)
    );
    ImGui::SetNextWindowSize(
        ImVec2(Layout::PanelWidth, Layout::PerformanceHeight),
        ImGuiCond_Always
    );

    pushProfilerStyle();
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
    popProfilerStyle();
}

void drawCameraPanel(State& state, Context& context)
{
    if (!state.show_camera) return;

    const Renderer::Transform *transform =
        context.world.get<Renderer::Transform>(context.camera);
    if (!transform) return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(
            io.DisplaySize.x - Layout::PanelPadding,
            Layout::CameraPanelTop(state.show_fps, ImGui::GetFrameHeight())
        ),
        ImGuiCond_Always,
        ImVec2(1.0f, 0.0f)
    );
    ImGui::SetNextWindowSize(
        ImVec2(Layout::PanelWidth, Layout::CameraHeight),
        ImGuiCond_Always
    );

    pushProfilerStyle();
    if (ImGui::Begin(
            "Camera",
            &state.show_camera,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(heading_color, "%s", "Position");
        ImGui::Text("X     %8.3f", transform->position.x);
        ImGui::Text("Y     %8.3f", transform->position.y);
        ImGui::Text("Z     %8.3f", transform->position.z);

        ImGui::Separator();
        ImGui::TextColored(heading_color, "%s", "Rotation");
        ImGui::Text("Pitch %8.2f deg", transform->rotation.x);
        ImGui::Text("Yaw   %8.2f deg", transform->rotation.y);
        ImGui::Text("Roll  %8.2f deg", transform->rotation.z);
    }
    ImGui::End();
    popProfilerStyle();
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
    drawCameraPanel(state, context);
}

} // namespace Debugging
