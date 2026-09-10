#include "Scenes/Driving.hpp"

#include "Driving/Camera.hpp"
#include "Driving/Components.hpp"
#include "Driving/Dynamics.hpp"
#include "Driving/Geometry.hpp"
#include "Driving/Traffic.hpp"

#include "Sources/Camera.hpp"
#include "Sources/Renderer/Components.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>

#include <cstdint>

namespace Game::Scenes {
namespace {

std::size_t triangles(Models::MeshHandle mesh)
{
    const Models::MeshData *data = Models::mesh(mesh);
    return data ? data->indices.size() / 3u : 0u;
}

} // namespace

const char *Driving::name() const
{
    return "Driving";
}

bool Driving::load(Ecs::World& world, std::string& error)
{
    camera_ = Ecs::INVALID_ENTITY;
    player_ = Ecs::INVALID_ENTITY;
    triangle_count_ = 0u;
    traffic_materials_.clear();
    traffic_system_.setSeed(0x48151623u);

    road_mesh_ = Game::Driving::Geometry::road(road_width_, segment_length_);
    marking_mesh_ = Game::Driving::Geometry::laneMarkings(lane_count_, lane_width_, segment_length_);
    box_mesh_ = Game::Driving::Geometry::unitBox();
    asphalt_ = Game::Driving::Geometry::material({0.055f, 0.060f, 0.065f});
    markings_ = Game::Driving::Geometry::material({0.88f, 0.88f, 0.82f});
    concrete_ = Game::Driving::Geometry::material({0.43f, 0.44f, 0.43f});
    traffic_materials_ = {
        Game::Driving::Geometry::material({0.55f, 0.06f, 0.04f}),
        Game::Driving::Geometry::material({0.05f, 0.10f, 0.16f}),
        Game::Driving::Geometry::material({0.72f, 0.72f, 0.69f}),
        Game::Driving::Geometry::material({0.05f, 0.05f, 0.05f}),
        Game::Driving::Geometry::material({0.22f, 0.25f, 0.20f}),
    };

    if (road_mesh_ == Models::INVALID_MESH || marking_mesh_ == Models::INVALID_MESH ||
        box_mesh_ == Models::INVALID_MESH || asphalt_ == Models::INVALID_MATERIAL ||
        markings_ == Models::INVALID_MATERIAL || concrete_ == Models::INVALID_MATERIAL)
    {
        error = "failed to create driving scene geometry";
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
    vehicle.road_half_width = road_width_ * 0.5f;
    world.add<Game::Driving::Vehicle>(player_, vehicle);

    camera_ = world.createEntity();
    world.add<Renderer::Transform>(camera_, Renderer::Transform{
        .position = {0.0f, 0.38f, 18.35f},
        .rotation = {-8.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f},
    });
    world.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
        101.0f, 0.025f, true
    });
    Game::Driving::DrivingCamera driving_camera;
    driving_camera.target = player_;
    world.add<Game::Driving::DrivingCamera>(camera_, driving_camera);

    createRoad(world);
    createTraffic(world);

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

    const Ecs::Entity gi = world.createEntity();
    world.add<Renderer::GlobalIlluminationComponent>(gi, Renderer::GlobalIlluminationComponent{
        .enabled = false,
        .intensity = 0.65f,
        .bounces = 1,
        .photon_mapping = false,
        .photon_count = 0u,
        .photon_radius = 0.0f,
    });

    world.markChanged();
    return true;
}

void Driving::addRenderable(
    Ecs::World& world,
    Models::MeshHandle mesh,
    Models::MaterialHandle material,
    const Renderer::Transform& transform,
    bool road_piece)
{
    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::Transform>(entity, transform);
    world.add<Renderer::MeshComponent>(entity, Renderer::MeshComponent{mesh, material});
    world.add<Renderer::RenderableComponent>(entity, Renderer::RenderableComponent{true});
    if (road_piece) world.add<Game::Driving::RoadPiece>(entity, Game::Driving::RoadPiece{});
    triangle_count_ += triangles(mesh);
}

void Driving::createRoad(Ecs::World& world)
{
    const float first = 6.0f * segment_length_;
    const float barrier_x = road_width_ * 0.5f + 0.18f;

    for (int index = 0; index < segment_count_; ++index) {
        const float z = first - static_cast<float>(index) * segment_length_;

        addRenderable(
            world,
            road_mesh_,
            asphalt_,
            Renderer::Transform{.position = {0.0f, 0.0f, z}, .rotation = {}, .scale = {1.0f, 1.0f, 1.0f}},
            true
        );
        addRenderable(
            world,
            marking_mesh_,
            markings_,
            Renderer::Transform{.position = {0.0f, 0.0f, z}, .rotation = {}, .scale = {1.0f, 1.0f, 1.0f}},
            true
        );
        addRenderable(
            world,
            box_mesh_,
            concrete_,
            Renderer::Transform{.position = {-barrier_x, 0.35f, z}, .rotation = {}, .scale = {0.34f, 0.70f, segment_length_}},
            true
        );
        addRenderable(
            world,
            box_mesh_,
            concrete_,
            Renderer::Transform{.position = {barrier_x, 0.35f, z}, .rotation = {}, .scale = {0.34f, 0.70f, segment_length_}},
            true
        );
    }
}

void Driving::createTraffic(Ecs::World& world)
{
    std::uint32_t state = 0x9e3779b9u;
    auto random01 = [&state]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>((state >> 8u) & 0x00ffffffu) / 16777215.0f;
    };

    for (int index = 0; index < traffic_count_; ++index) {
        const bool heavy = index % 9 == 0;
        const int lane = index % lane_count_;
        const float z = -65.0f - static_cast<float>(index) * 28.0f - random01() * 80.0f;
        const float speed = heavy ? 23.0f + random01() * 7.0f : 27.0f + random01() * 13.0f;
        const float length = heavy ? 10.5f : 4.4f + random01() * 0.8f;
        const float width = heavy ? 2.5f : 1.82f;
        const float height = heavy ? 3.35f : 1.35f;

        const Ecs::Entity entity = world.createEntity();
        world.add<Renderer::Transform>(entity, Renderer::Transform{
            .position = {Game::Driving::laneCenter(lane, lane_count_, lane_width_), height * 0.5f, z},
            .rotation = {},
            .scale = {width, height, length},
        });
        world.add<Renderer::MeshComponent>(entity, Renderer::MeshComponent{
            box_mesh_,
            traffic_materials_[static_cast<std::size_t>(index) % traffic_materials_.size()]
        });
        world.add<Renderer::RenderableComponent>(entity, Renderer::RenderableComponent{true});

        Game::Driving::Traffic traffic;
        traffic.lane = lane;
        traffic.target_lane = lane;
        traffic.speed = speed;
        traffic.desired_speed = speed;
        traffic.length = length;
        traffic.lane_change_speed = heavy ? 1.15f : 1.80f;
        traffic.follow_distance = heavy ? 48.0f : 34.0f;
        traffic.lane_change_cooldown = random01() * 3.0f;
        traffic.heavy = heavy;
        world.add<Game::Driving::Traffic>(entity, traffic);
        triangle_count_ += triangles(box_mesh_);
    }
}

void Driving::recycleRoad(Ecs::World& world)
{
    const Renderer::Transform *player = world.get<Renderer::Transform>(player_);
    if (!player) return;

    const float recycle_behind = segment_length_ * 6.0f;
    const float span = segment_length_ * static_cast<float>(segment_count_);
    bool changed = false;

    world.each<Game::Driving::RoadPiece, Renderer::Transform>(
        [&](Ecs::Entity, Game::Driving::RoadPiece&, Renderer::Transform& transform) {
            while (transform.position.z > player->position.z + recycle_behind) {
                transform.position.z -= span;
                changed = true;
            }
        }
    );

    if (changed) world.markChanged();
}

void Driving::update(Ecs::World& world, float delta_seconds)
{
    const bool input_enabled = !::UI::wantsKeyboard();
    Game::Driving::updatePlayer(world, delta_seconds, input_enabled);
    traffic_system_.update(
        world,
        player_,
        delta_seconds,
        lane_width_,
        lane_count_,
        traffic_spawn_ahead_,
        traffic_despawn_behind_
    );
    recycleRoad(world);
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
    Game::Driving::Vehicle *vehicle = world.get<Game::Driving::Vehicle>(player_);
    Game::Driving::DrivingCamera *camera = world.get<Game::Driving::DrivingCamera>(camera_);
    const Renderer::Transform *player_transform = world.get<Renderer::Transform>(player_);
    if (!vehicle || !camera || !player_transform) return;

    const Game::Driving::TrafficSystem::Statistics& traffic = traffic_system_.statistics();

    ImGui::SetNextWindowPos(ImVec2(8.0f, 350.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350.0f, 390.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Driving")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Speed: %.1f km/h", vehicle->speed * 3.6f);
    ImGui::Text("Steering: %.2f", vehicle->steering);
    ImGui::Text("Throttle / brake: %.2f / %.2f", vehicle->throttle, vehicle->brake);
    ImGui::Text("Position: %.1f, %.1f", player_transform->position.x, player_transform->position.z);

    ImGui::SeparatorText("Traffic");
    ImGui::Text("Vehicles: %zu", traffic.count);
    ImGui::Text("Average speed: %.1f km/h", traffic.average_speed * 3.6f);
    ImGui::Text("Nearest ahead: %.1f m", traffic.nearest_ahead);
    ImGui::Text("Lane changes: %llu", static_cast<unsigned long long>(traffic.lane_changes));
    ImGui::DragFloat("Spawn ahead", &traffic_spawn_ahead_, 5.0f, 100.0f, 2000.0f, "%.0f m");
    ImGui::DragFloat("Despawn behind", &traffic_despawn_behind_, 2.0f, 20.0f, 500.0f, "%.0f m");

    ImGui::SeparatorText("Vehicle");
    ImGui::DragFloat("Max speed", &vehicle->maximum_speed, 0.25f, 10.0f, 140.0f, "%.2f m/s");
    ImGui::DragFloat("Acceleration", &vehicle->engine_acceleration, 0.1f, 0.1f, 40.0f);
    ImGui::DragFloat("Braking", &vehicle->brake_deceleration, 0.1f, 0.1f, 50.0f);
    ImGui::DragFloat("Steering angle", &vehicle->maximum_steering_degrees, 0.1f, 1.0f, 60.0f);
    ImGui::DragFloat("Steering response", &vehicle->steering_response, 0.05f, 0.1f, 20.0f);

    ImGui::SeparatorText("Camera");
    ImGui::DragFloat("Height", &camera->height, 0.005f, 0.05f, 2.0f);
    ImGui::DragFloat("Forward offset", &camera->forward_offset, 0.01f, -2.0f, 5.0f);
    ImGui::DragFloat("Pitch", &camera->pitch_degrees, 0.1f, -30.0f, 20.0f);
    ImGui::DragFloat("Low FOV", &camera->low_speed_fov, 0.1f, 50.0f, 140.0f);
    ImGui::DragFloat("High FOV", &camera->high_speed_fov, 0.1f, 50.0f, 150.0f);

    ImGui::End();
}

void Driving::emitMetrics(const std::function<void(std::string_view, double)>& emit) const
{
    if (!emit) return;

    const Game::Driving::TrafficSystem::Statistics& traffic = traffic_system_.statistics();
    emit("driving_traffic_entities", static_cast<double>(traffic.count));
    emit("driving_traffic_average_speed", static_cast<double>(traffic.average_speed));
    emit("driving_nearest_traffic", static_cast<double>(traffic.nearest_ahead));
    emit("driving_lane_changes", static_cast<double>(traffic.lane_changes));
    emit("driving_scene_triangles", static_cast<double>(triangle_count_));
}

} // namespace Game::Scenes
