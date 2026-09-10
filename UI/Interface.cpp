#include "UI/Interface.hpp"

#include "Sources/Renderer/GlobalIllumination/Debug.hpp"
#include "Sources/Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Sources/Renderer/PathTracer/PathTracer.hpp"
#include "Sources/Renderer/Rasterizer/Rasterizer.hpp"
#include "Sources/Renderer/Visibility/Visibility.hpp"

#include <imgui.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Game::UI {
namespace {

Renderer::GlobalIlluminationComponent *globalIllumination(Ecs::World& world)
{
    for (const Ecs::Entity entity : world.entities()) {
        if (auto *component = world.get<Renderer::GlobalIlluminationComponent>(entity))
            return component;
    }
    return nullptr;
}

} // namespace

void Interface::setApproximationWindow(float x, float y, float width, float height)
{
    approximation_layout_ = {x, y, width, height};
}

void Interface::setSceneWindow(float x, float y, float width, float height)
{
    scene_layout_ = {x, y, width, height};
}

void Interface::setInformationWindow(float x, float y, float width, float height)
{
    information_layout_ = {x, y, width, height};
}

void Interface::setDebugWindow(float x, float y, float width, float height)
{
    debug_layout_ = {x, y, width, height};
}

void Interface::setTooltip(float padding_x, float padding_y, float wrap_width)
{
    tooltip_padding_x_ = padding_x;
    tooltip_padding_y_ = padding_y;
    tooltip_wrap_width_ = wrap_width;
}

void Interface::setControlWidth(float width)
{
    control_width_ = width;
}

void Interface::addIntControl(
    Renderer::IRenderer& renderer,
    std::string label,
    std::function<int()> read,
    std::function<void(int)> write,
    int minimum,
    int maximum,
    std::string help_text)
{
    Control control;
    control.type = Control::Type::Integer;
    control.renderer = &renderer;
    control.label = std::move(label);
    control.help = std::move(help_text);
    control.read_int = std::move(read);
    control.write_int = std::move(write);
    control.int_minimum = minimum;
    control.int_maximum = maximum;
    controls_.push_back(std::move(control));
}

void Interface::addFloatControl(
    Renderer::IRenderer& renderer,
    std::string label,
    std::function<float()> read,
    std::function<void(float)> write,
    float minimum,
    float maximum,
    float speed,
    std::string help_text)
{
    Control control;
    control.type = Control::Type::Float;
    control.renderer = &renderer;
    control.label = std::move(label);
    control.help = std::move(help_text);
    control.read_float = std::move(read);
    control.write_float = std::move(write);
    control.float_minimum = minimum;
    control.float_maximum = maximum;
    control.speed = speed;
    controls_.push_back(std::move(control));
}

void Interface::help(const char *text) const
{
    if (!text || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(tooltip_padding_x_, tooltip_padding_y_)
    );
    ImGui::BeginTooltip();
    if (tooltip_wrap_width_ > 0.0f) ImGui::PushTextWrapPos(tooltip_wrap_width_);
    ImGui::TextUnformatted(text);
    if (tooltip_wrap_width_ > 0.0f) ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleVar();
}

void Interface::place(const Layout& layout) const
{
    ImGui::SetNextWindowPos(ImVec2(layout.x, layout.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_FirstUseEver);
}

void Interface::rendererControls(Renderer::Manager& renderers)
{
    Renderer::IRenderer *active = renderers.active();
    if (!active) return;

    for (Control& control : controls_) {
        if (control.renderer != active) continue;
        if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);

        if (control.type == Control::Type::Integer) {
            if (!control.read_int || !control.write_int) continue;
            int value = control.read_int();
            if (ImGui::SliderInt(
                    control.label.c_str(),
                    &value,
                    control.int_minimum,
                    control.int_maximum))
            {
                control.write_int(value);
            }
        } else {
            if (!control.read_float || !control.write_float) continue;
            float value = control.read_float();
            if (ImGui::DragFloat(
                    control.label.c_str(),
                    &value,
                    control.speed,
                    control.float_minimum,
                    control.float_maximum))
            {
                control.write_float(value);
            }
        }
        help(control.help.c_str());
    }
}

void Interface::approximation(Ecs::World& world, Renderer::Manager& renderers)
{
    place(approximation_layout_);
    if (!ImGui::Begin("Rendering")) {
        ImGui::End();
        return;
    }

    if (Renderer::GlobalIlluminationComponent *gi = globalIllumination(world)) {
        bool enabled = gi->enabled;
        if (ImGui::Checkbox("Global Illumination", &enabled)) {
            gi->enabled = enabled;
            world.markChanged();
        }
        help("Enable or disable indirect lighting for the current scene.");

        if (gi->enabled) {
            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float intensity = gi->intensity;
            if (ImGui::DragFloat("GI Intensity", &intensity, 0.01f, 0.0f, 4.0f)) {
                gi->intensity = intensity;
                world.markChanged();
            }
            help("Scales the indirect light contribution.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            int bounces = static_cast<int>(gi->bounces);
            if (ImGui::SliderInt("GI Bounces", &bounces, 1, 4)) {
                gi->bounces = static_cast<std::uint8_t>(bounces);
                world.markChanged();
            }
            help("Number of indirect-light bounce passes used by GI.");

            bool photons = gi->photon_mapping;
            if (ImGui::Checkbox("Photon Mapping", &photons)) {
                gi->photon_mapping = photons;
                world.markChanged();
            }
            help("Use the photon map as the indirect-light source.");

            if (photons) {
                if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
                int photon_count = static_cast<int>(std::min<std::uint32_t>(
                    gi->photon_count,
                    static_cast<std::uint32_t>(2147483647)
                ));
                if (ImGui::InputInt("Photons", &photon_count)) {
                    gi->photon_count = static_cast<std::uint32_t>(std::max(photon_count, 0));
                    world.markChanged();
                }
                help("Number of photons emitted when the photon map is rebuilt.");

                if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
                float radius = gi->photon_radius;
                if (ImGui::DragFloat("Photon Radius", &radius, 0.001f, 0.0f, 0.0f)) {
                    gi->photon_radius = std::max(radius, 0.0f);
                    world.markChanged();
                }
                help("Photon gather radius. Zero lets Horse derive it from the scene.");
            }
        }
    }

    rendererControls(renderers);
    ImGui::End();
}

std::size_t Interface::sceneManager(Scenes::Manager& scenes, Renderer::Manager& renderers)
{
    std::size_t requested_scene = scenes.currentIndex();

    place(scene_layout_);
    if (!ImGui::Begin("Scene Manager")) {
        ImGui::End();
        return requested_scene;
    }

    const Scenes::Scene *current_scene = scenes.current();
    const char *scene_name = current_scene ? current_scene->name() : "None";
    if (ImGui::BeginCombo("Scene", scene_name)) {
        for (std::size_t index = 0u; index < scenes.count(); ++index) {
            const Scenes::Scene *scene = scenes.at(index);
            if (!scene) continue;
            const bool selected = index == scenes.currentIndex();
            if (ImGui::Selectable(scene->name(), selected)) requested_scene = index;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    help("Load a different GAME scene.");

    const Renderer::Manager::Entry *active = renderers.activeEntry();
    const char *renderer_name = active ? active->name.c_str() : "None";
    if (ImGui::BeginCombo("Renderer", renderer_name)) {
        for (std::size_t index = 0u; index < renderers.count(); ++index) {
            const Renderer::Manager::Entry *entry = renderers.entry(index);
            if (!entry) continue;
            const bool selected = index == renderers.activeIndex();
            if (!entry->available) ImGui::BeginDisabled();
            if (ImGui::Selectable(entry->name.c_str(), selected) && entry->available)
                renderers.activate(index);
            if (selected) ImGui::SetItemDefaultFocus();
            if (!entry->available) ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    help("Switch between the renderers registered by GAME.");

    ImGui::End();
    return requested_scene;
}

void Interface::information(
    Ecs::World& world,
    Renderer::Manager& renderers,
    Renderer::Debug::Inspector& inspector)
{
    place(information_layout_);
    if (!ImGui::Begin("Informations")) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Frame");
    const Renderer::Manager::Entry *active = renderers.activeEntry();
    const ImGuiIO& io = ImGui::GetIO();
    const float fps = io.Framerate;
    const float frame_ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
    ImGui::Text("Renderer: %s", active ? active->name.c_str() : "None");
    ImGui::Text("Resolution: %d x %d", std::max(Display.getWidth(), 1), std::max(Display.getHeight(), 1));
    ImGui::Text("Frame: %.1f FPS / %.2f ms", fps, frame_ms);

    auto *active_rasterizer = dynamic_cast<Renderer::Rasterizer *>(renderers.active());
    if (active_rasterizer) {
        ImGui::SeparatorText("Rasterizer Pipeline");
        const Renderer::RasterizerStatistics raster_stats = active_rasterizer->statistics();
        const Renderer::HorizonGI::Statistics horizon_stats = active_rasterizer->horizonGiStatistics();
        const Renderer::Upscale::Statistics upscale_stats = active_rasterizer->upscaleStatistics();
        const Renderer::HorizonGI::Settings& horizon_settings = active_rasterizer->horizonGiSettings();

        ImGui::Text("Scaled pipeline: %s", raster_stats.scaled_pipeline_active ? "ACTIVE" : "DIRECT");
        ImGui::Text("Output: %d x %d", raster_stats.output_width, raster_stats.output_height);
        ImGui::Text("Lighting: %d x %d (1/%d)",
            raster_stats.lighting_width,
            raster_stats.lighting_height,
            active_rasterizer->lightingResolutionDivisor());
        ImGui::Text("Depth prepass: %zu draw items", raster_stats.depth_prepass_items);
        ImGui::Text("Shadows: %s / %d px / divisor %d",
            raster_stats.shadow_active ? "ACTIVE" : "INACTIVE",
            raster_stats.shadow_resolution,
            active_rasterizer->shadowResolutionDivisor());

        ImGui::Text("Horizon GI/AO: %s", horizon_stats.active ? "ACTIVE" : "INACTIVE");
        if (horizon_stats.active) {
            ImGui::Text("Horizon resolution: %d x %d (1/%d)",
                horizon_stats.width,
                horizon_stats.height,
                horizon_settings.pass.resolution_divisor);
            ImGui::Text("Horizon samples: %d directions x %d steps",
                horizon_stats.directions, horizon_stats.steps);
            ImGui::Text("Horizon indirect: %s / AO: same pass",
                horizon_stats.indirect ? "ON" : "OFF");
            ImGui::Text("Horizon history: %s",
                horizon_stats.temporal_history ? "VALID" : "RESET / UNUSED");
        }

        ImGui::Text("Upscale: %s", upscale_stats.active ? "ACTIVE" : "INACTIVE");
        if (upscale_stats.active) {
            ImGui::Text("Upscale: %d x %d -> %d x %d",
                upscale_stats.source_width,
                upscale_stats.source_height,
                upscale_stats.output_width,
                upscale_stats.output_height);
            ImGui::Text("Upscale depth-aware: %s",
                upscale_stats.depth_aware ? "ON" : "OFF");
            ImGui::Text("Upscale effect compose: %s",
                upscale_stats.effect ? "ON" : "OFF");
            ImGui::Text("Upscale history: %s",
                upscale_stats.temporal_history ? "VALID" : "RESET / UNUSED");
        }
    }

    if (auto *active_path_tracer = dynamic_cast<Renderer::PathTracer *>(renderers.active())) {
        const Renderer::PathTracerStatistics path = active_path_tracer->statistics();
        ImGui::SeparatorText("Path Tracer Scheduling");
        ImGui::Text("Output: %d x %d", path.output_width, path.output_height);
        ImGui::Text("Trace: %d x %d (1/%d)",
            path.trace_width,
            path.trace_height,
            active_path_tracer->resolutionDivisor());
        ImGui::Text("Samples / frame: %d", path.samples_per_frame);
        ImGui::Text("Phase grids: stationary %d / reset %d / moving %d",
            path.stationary_phase_grid,
            path.reset_phase_grid,
            path.moving_phase_grid);
        ImGui::Text("Moving depth block: %d", path.moving_depth_block);
        ImGui::Text("Path pixels: stationary %llu / reset %llu / moving %llu",
            static_cast<unsigned long long>(path.stationary_path_pixel_budget),
            static_cast<unsigned long long>(path.reset_path_pixel_budget),
            static_cast<unsigned long long>(path.moving_path_pixel_budget));
        ImGui::Text("Moving depth rays: %llu",
            static_cast<unsigned long long>(path.moving_depth_ray_budget));
    }

    ImGui::SeparatorText("Viewport");
    const Renderer::Visibility::Result visibility = Renderer::Visibility::system().evaluate(
        world,
        std::max(Display.getWidth(), 1),
        std::max(Display.getHeight(), 1)
    );
    const std::size_t entity_total = visibility.visible.size() + visibility.culled.size();
    const std::size_t triangle_total = visibility.visible_triangles + visibility.culled_triangles;
    const float entity_culled = entity_total > 0u
        ? 100.0f * static_cast<float>(visibility.culled.size()) / static_cast<float>(entity_total)
        : 0.0f;
    const float triangle_culled = triangle_total > 0u
        ? 100.0f * static_cast<float>(visibility.culled_triangles) / static_cast<float>(triangle_total)
        : 0.0f;
    ImGui::Text("Entities: %zu visible / %zu culled (%.1f%%)",
        visibility.visible.size(), visibility.culled.size(), entity_culled);
    ImGui::Text("Triangles: %zu visible / %zu culled (%.1f%%)",
        visibility.visible_triangles, visibility.culled_triangles, triangle_culled);
    ImGui::Text("Far distance: %.1f", Renderer::Visibility::system().farDistance());

    const Renderer::Debug::SnapshotInfo snapshot = inspector.snapshotInfo();
    ImGui::Text("Snapshot: %s", snapshot.frozen ? "Frozen" : "Live");
    if (snapshot.frozen) {
        ImGui::Text("Frozen entities: %zu visible / %zu culled",
            snapshot.visible_entities, snapshot.culled_entities);
        ImGui::Text("Frozen triangles: %zu visible / %zu culled",
            snapshot.visible_triangles, snapshot.culled_triangles);
    }

    ImGui::SeparatorText("Debug");
    ImGui::Text("BVH overlay: %s", inspector.showBvh() ? "ON" : "OFF");
    ImGui::Text("Viewport overlay: %s", inspector.showViewport() ? "ON" : "OFF");
    const Renderer::Debug::BvhInfo bvh = inspector.bvhInfo();
    if (bvh.available) {
        ImGui::Text("BVH level: %d / %d", bvh.level, bvh.maximum_level);
        ImGui::Text("BVH nodes: %zu total / %zu on level", bvh.total_nodes, bvh.level_nodes);
        if (bvh.selected) {
            ImGui::Text("Containing nodes: %zu / selected #%zu",
                bvh.containing_nodes, bvh.selected_node);
        } else {
            ImGui::Text("Containing nodes: 0 / selected none");
        }
    } else {
        ImGui::Text("BVH selection: unavailable");
    }

    ImGui::SeparatorText("GI / Photon Mapping");
    const Renderer::GlobalIllumination::Debug::Statistics gi_stats =
        Renderer::GlobalIllumination::Debug::statistics();
    const Renderer::GlobalIlluminationComponent *gi = globalIllumination(world);
    const bool gi_enabled = gi && gi->enabled;
    const bool photon_requested = gi_enabled && gi->photon_mapping && gi->photon_count > 0u;
    const bool horizon_replaces_field = active_rasterizer && active_rasterizer->horizonGiSettings().enabled;
    const bool photon_active = photon_requested &&
        Renderer::GlobalIllumination::Debug::photonMap().valid();
    ImGui::Text("Renderer GI source: %s", horizon_replaces_field
        ? "Horizon screen-space GI + AO"
        : "Probe / SH field");
    ImGui::Text("Probe GI: %s", horizon_replaces_field
        ? "BYPASSED"
        : (gi_enabled ? (gi_stats.calculating ? "Calculating" : "Ready") : "Disabled"));
    ImGui::Text("GI progress: %.1f%%", std::clamp(gi_stats.progress, 0.0f, 1.0f) * 100.0f);
    ImGui::Text("GI probes: %zu", gi_stats.probes);
    ImGui::Text("GI bounce: %u / %u",
        static_cast<unsigned>(gi_stats.bounce), static_cast<unsigned>(gi_stats.bounces));
    ImGui::Text("GI scene: %zu triangles / %zu materials",
        gi_stats.triangles, gi_stats.materials);
    ImGui::Text("GI BVH: %zu nodes / depth %u", gi_stats.bvh_nodes, gi_stats.bvh_depth);
    ImGui::Text("GI scene build: %.2f ms", gi_stats.scene_build_ms);
    ImGui::Text("Photon Mapping: %s", horizon_replaces_field
        ? "BYPASSED"
        : (photon_active ? "ACTIVE" : (photon_requested ? "BUILDING / EMPTY" : "INACTIVE")));
    ImGui::Text("Photons: %zu stored / %u requested",
        gi_stats.photons, gi_stats.requested_photons);
    ImGui::Text("Photon radius: %.4f", gi_stats.photon_radius);
    ImGui::Text("Photon build: %.2f ms", gi_stats.photon_build_ms);

    ImGui::End();
}

void Interface::debug(
    Ecs::World& world,
    Renderer::Manager& renderers,
    Renderer::Debug::Inspector& inspector)
{
    place(debug_layout_);
    if (!ImGui::Begin("Debug View")) {
        ImGui::End();
        return;
    }

    if (auto *rasterizer = dynamic_cast<Renderer::Rasterizer *>(renderers.active())) {
        ImGui::SeparatorText("Rasterizer Quality");

        bool viewport_culling = rasterizer->viewportCulling();
        if (ImGui::Checkbox("Viewport Culling", &viewport_culling))
            rasterizer->setViewportCulling(viewport_culling);
        help("Cull rasterized camera geometry outside the current viewport. Shadow casters stay available to lighting.");

        if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
        int lighting_divisor = rasterizer->lightingResolutionDivisor();
        if (ImGui::SliderInt("Lighting Resolution Divisor", &lighting_divisor, 1, 8))
            rasterizer->setLightingResolutionDivisor(lighting_divisor);
        help("Render direct lighting at 1/N of output resolution before reconstruction.");

        bool depth_aware = rasterizer->depthAwareUpscaling();
        if (ImGui::Checkbox("Depth-aware Upscaling", &depth_aware))
            rasterizer->setDepthAwareUpscaling(depth_aware);
        help("Use full-resolution depth to keep reduced-resolution lighting edges aligned with geometry.");

        bool temporal_upscale = rasterizer->temporalUpscaling();
        if (ImGui::Checkbox("Temporal Upscaling", &temporal_upscale))
            rasterizer->setTemporalUpscaling(temporal_upscale);
        help("Blend stable reconstructed lighting with valid previous-frame history.");

        if (temporal_upscale) {
            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float temporal_weight = rasterizer->temporalUpscalingWeight();
            if (ImGui::SliderFloat("Upscale History Weight", &temporal_weight, 0.0f, 0.98f))
                rasterizer->setTemporalUpscalingWeight(temporal_weight);
            help("Amount of valid previous-frame color kept by temporal reconstruction.");
        }

        if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
        float depth_threshold = rasterizer->upscalingDepthThreshold();
        if (ImGui::DragFloat("Upscale Depth Threshold", &depth_threshold, 0.001f, 0.0f, 1.0f))
            rasterizer->setUpscalingDepthThreshold(depth_threshold);
        help("Maximum relative depth difference accepted when reconstructing low-resolution samples.");

        if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
        int shadow_divisor = rasterizer->shadowResolutionDivisor();
        if (ImGui::SliderInt("Shadow Resolution Divisor", &shadow_divisor, 1, 16))
            rasterizer->setShadowResolutionDivisor(shadow_divisor);
        help("Render point-light shadow faces at a fraction of output resolution, capped by the configured maximum.");

        ImGui::SeparatorText("Horizon GI + AO");
        Renderer::HorizonGI::Settings& horizon = rasterizer->horizonGiSettings();
        bool horizon_enabled = horizon.enabled;
        if (ImGui::Checkbox("Horizon GI", &horizon_enabled))
            rasterizer->setHorizonGiEnabled(horizon_enabled);
        help("Low-end screen-space GI. AO is produced by the same horizon traversal with no separate AO pass.");

        if (horizon_enabled) {
            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            int horizon_divisor = horizon.pass.resolution_divisor;
            if (ImGui::SliderInt("Horizon Resolution Divisor", &horizon_divisor, 1, 8))
                rasterizer->setHorizonGiResolutionDivisor(horizon_divisor);
            help("Run Horizon GI/AO at 1/N of the output resolution.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            int directions = horizon.directions;
            if (ImGui::SliderInt("Horizon Directions", &directions, 1, Renderer::HorizonGI::MaximumDirections))
                rasterizer->setHorizonGiDirections(directions);
            help("Angular directions sampled around each pixel.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            int steps = horizon.steps;
            if (ImGui::SliderInt("Horizon Steps", &steps, 1, Renderer::HorizonGI::MaximumSteps))
                rasterizer->setHorizonGiSteps(steps);
            help("Depth samples taken per horizon direction.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float radius = horizon.radius;
            if (ImGui::DragFloat("Horizon Radius", &radius, 0.05f, 0.05f, 20.0f))
                rasterizer->setHorizonGiRadius(radius);
            help("World-space search radius reconstructed from depth.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float thickness = horizon.thickness;
            if (ImGui::DragFloat("Horizon Thickness", &thickness, 0.01f, 0.0f, 5.0f))
                rasterizer->setHorizonGiThickness(thickness);
            help("Depth thickness tolerance used to reduce false occlusion.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float ao_strength = horizon.ao_strength;
            if (ImGui::DragFloat("Horizon AO Strength", &ao_strength, 0.02f, 0.0f, 4.0f))
                rasterizer->setHorizonGiAoStrength(ao_strength);
            help("Ambient-occlusion strength from the same traversal used for indirect light.");

            if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
            float indirect_strength = horizon.indirect_strength;
            if (ImGui::DragFloat("Horizon Indirect Strength", &indirect_strength, 0.02f, 0.0f, 4.0f))
                rasterizer->setHorizonGiIndirectStrength(indirect_strength);
            help("Diffuse screen-space bounce-light contribution.");

            bool horizon_temporal = horizon.pass.temporal_filter;
            if (ImGui::Checkbox("Horizon Temporal Filter", &horizon_temporal))
                rasterizer->setHorizonGiTemporalFilter(horizon_temporal);
            help("Reuse valid Horizon GI/AO history while camera and scene signatures remain stable.");

            if (horizon_temporal) {
                if (control_width_ > 0.0f) ImGui::SetNextItemWidth(control_width_);
                float horizon_weight = horizon.pass.temporal_weight;
                if (ImGui::SliderFloat("Horizon History Weight", &horizon_weight, 0.0f, 0.98f))
                    rasterizer->setHorizonGiTemporalWeight(horizon_weight);
                help("Amount of valid Horizon GI/AO history kept each frame.");
            }
        }
    }

    ImGui::SeparatorText("Inspector");

    bool bvh = inspector.showBvh();
    if (ImGui::Checkbox("BVH", &bvh)) inspector.setShowBvh(bvh);
    help("Draw the BVH level used to inspect scene partitioning. The relevant box is highlighted.");

    if (bvh) {
        int level = inspector.bvhLevel();
        if (ImGui::InputInt("BVH Level", &level)) inspector.setBvhLevel(std::max(level, 0));
        help("Choose which BVH depth is drawn.");
    }

    bool viewport = inspector.showViewport();
    if (ImGui::Checkbox("Viewport", &viewport)) inspector.setShowViewport(viewport);
    help("Draw the player camera frustum used for viewport visibility.");

    bool frozen = inspector.frozen();
    if (ImGui::Checkbox("Freeze All Updates", &frozen)) {
        if (frozen) {
            if (!inspector.freeze(
                    world,
                    std::max(Display.getWidth(), 1),
                    std::max(Display.getHeight(), 1)))
            {
                frozen = false;
            }
        } else {
            inspector.unfreeze(world);
        }
    }
    help("Freeze scene, animation, GI and visibility state, then move a separate debug camera around the captured player viewport.");

    float opacity = inspector.overlayOpacity();
    if (ImGui::SliderFloat("Overlay Opacity", &opacity, 0.0f, 1.0f))
        inspector.setOverlayOpacity(opacity);
    help("Opacity of the BVH, frozen viewport and player markers.");

    ImGui::End();
}

std::size_t Interface::draw(
    Ecs::World& world,
    Scenes::Manager& scenes,
    Renderer::Manager& renderers,
    Renderer::Debug::Inspector& inspector)
{
    approximation(world, renderers);
    const std::size_t requested_scene = sceneManager(scenes, renderers);
    information(world, renderers, inspector);
    debug(world, renderers, inspector);
    return requested_scene;
}

} // namespace Game::UI
