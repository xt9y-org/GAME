#include "UI/Interface.hpp"

#include "Sources/Renderer/GlobalIllumination/GlobalIllumination.hpp"

#include <imgui.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <cstdint>

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

void Interface::information(Renderer::Debug::Inspector& inspector)
{
    place(information_layout_);
    if (!ImGui::Begin("Informations")) {
        ImGui::End();
        return;
    }

    const Renderer::Debug::SnapshotInfo info = inspector.snapshotInfo();
    ImGui::Text("Snapshot: %s", info.frozen ? "Frozen" : "Live");
    if (info.frozen) {
        ImGui::Text("Visible entities: %zu", info.visible_entities);
        ImGui::Text("Culled entities: %zu", info.culled_entities);
        ImGui::Text("Visible triangles: %zu", info.visible_triangles);
        ImGui::Text("Culled triangles: %zu", info.culled_triangles);
    }

    ImGui::End();
}

void Interface::debug(Ecs::World& world, Renderer::Debug::Inspector& inspector)
{
    place(debug_layout_);
    if (!ImGui::Begin("Debug View")) {
        ImGui::End();
        return;
    }

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
    information(inspector);
    debug(world, inspector);
    return requested_scene;
}

} // namespace Game::UI
