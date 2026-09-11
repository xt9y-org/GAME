#include "Scenes/Driving.hpp"

#include "Driving/Camera.hpp"
#include "Driving/Components.hpp"
#include "Driving/Dynamics.hpp"

#include "Sources/Camera.hpp"
#include "Sources/Renderer/Components.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>

#include <algorithm>

namespace Game::Scenes {

const char *Driving::name() const
{
    return "Driving";
}

bool Driving::load(Ecs::World& world, std::string& error)
{
    camera_ = Ecs::INVALID_ENTITY;
    player_ = Ecs::INVALID_ENTITY;
    triangle_count_ = 0u;
    road_half_width_ = 0.0f;

    if (!assets_.load(error)) return false;

    road_half_width_ = Game::Driving::Assets::halfWidth(
        assets_.street,
        street_segment_length_
    );
    if (road_half_width_ <= 0.5f) {
        error = "failed to trace usable highway width from model bounds";
        return false;
    }

    player_ = world.createEntity();
    world.add<Renderer::Transform>(player_, Renderer::Transform{
        .position = {0.0f, 0.0f, 20.0f},
        .rotation = {},
        .scale = {1.0f, 1.0f, 1.0f},
    });
    world.add<Game::Driving::Player>(player_, Game::Driving::Player{});
    Game::Driving::Vehicle vehicle;
    vehicle.road_half_width = road_half_width_;
    world.add<Game::Driving::Vehicle>(player_, vehicle);

    camera_ = world.createEntity();
    world.add<Renderer::Transform>(camera_, Renderer::Transform{
        .position = {0.0f, 0.55f, 18.35f},
        .rotation = {-8.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f},
    });
    world.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
        101.0f, 0.025f, true
    });
    Game::Driving::DrivingCamera driving_camera;
    driving_camera.target = player_;
    world.add<Game::Driving::DrivingCamera>(camera_, driving_camera);

    if (!createStreet(world, error)) return false;

    const Ecs::Entity sun = world.createEntity();
    world.add<Renderer::Transform>(sun, Renderer::Transform{
        .position = {0.0f, 80.0f, 0.0f},
        .rotation = {-48.0f, 28.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f},
    });
    world.add<Renderer::LightComponent>(sun, Renderer::LightComponent{
        .type = Renderer::LightType::Directional,
        .color = {0.92f, 0.95f, 1.0f},
        .intensity = 2.1f,
    });

    world.markChanged();
    return true;
}

bool Driving::createStreet(Ecs::World& world, std::string& error)
{
    const float first = street_segment_length_ * 2.0f;

    for (int index = 0; index < street_segment_count_; ++index) {
        const Ecs::Entity root = world.createEntity();
        world.add<Renderer::Transform>(root, Renderer::Transform{
            .position = {
                0.0f,
                0.0f,
                first - static_cast<float>(index) * street_segment_length_,
            },
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Game::Driving::RoadPiece>(root, Game::Driving::RoadPiece{});

        const std::size_t triangles = Game::Driving::Assets::attach(
            world,
            root,
            assets_.street,
            street_segment_length_
        );
        if (triangles == 0u) {
            error = "failed to create street segment";
            return false;
        }
        triangle_count_ += triangles;
    }

    return true;
}

void Driving::recycleStreet(Ecs::World& world)
{
    const Renderer::Transform *player = world.get<Renderer::Transform>(player_);
    if (!player) return;

    const float recycle_behind = street_segment_length_ * 2.0f;
    const float span = street_segment_length_ * static_cast<float>(street_segment_count_);
    bool changed = false;

    world.each<Game::Driving::RoadPiece, Renderer::Transform>(
        [&](Ecs::Entity, Game::Driving::RoadPiece&, Renderer::Transform& transform) {
            while (transform.position.z > player->position.z + recycle_behind) {
                transform.position.z -= span;
                changed = true;
            }
        }
    );

    if (changed) world.markChanged(Ecs::ChangeKind::Transform);
}

void Driving::update(Ecs::World& world, float delta_seconds)
{
    const bool input_enabled = !::UI::wantsKeyboard();
    Game::Driving::updatePlayer(world, delta_seconds, input_enabled);
    recycleStreet(world);
    Game::Driving::updateCamera(world, delta_seconds);
}

Ecs::Entity Driving::camera() const
{
    return camera_;
}

std::size_t Driving::triangleCount() const
{
    return triangle_count_;
}

void Driving::drawDebug(Ecs::World& world)
{
    const Game::Driving::Vehicle *vehicle = world.get<Game::Driving::Vehicle>(player_);
    const Renderer::Transform *player_transform = world.get<Renderer::Transform>(player_);
    if (!vehicle || !player_transform) return;

    const float drive_limit = std::max(
        vehicle->road_half_width - vehicle->road_edge_margin,
        0.5f
    );

    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(285.0f, 165.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Driving Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Speed: %.1f km/h", vehicle->speed * 3.6f);
    ImGui::Text("Position: %.1f, %.1f", player_transform->position.x, player_transform->position.z);
    ImGui::Text("Street: %s", assets_.street.valid() ? "loaded" : "missing");
    ImGui::Text("Mesh edges: %.2f .. %.2f m", -vehicle->road_half_width, vehicle->road_half_width);
    ImGui::Text("Drive limits: %.2f .. %.2f m", -drive_limit, drive_limit);
    ImGui::Text("Segments: %d", street_segment_count_);
    ImGui::Text("Triangles: %zu", triangle_count_);

    ImGui::End();
}

void Driving::emitMetrics(const std::function<void(std::string_view, double)>& emit) const
{
    if (!emit) return;
    emit("driving_street_segments", static_cast<double>(street_segment_count_));
    emit("driving_road_half_width", static_cast<double>(road_half_width_));
    emit("driving_release_assets_loaded", static_cast<double>(assets_.loaded));
    emit("driving_scene_triangles", static_cast<double>(triangle_count_));
}

} // namespace Game::Scenes
