#include "Driving/Traffic.hpp"

#include "Driving/Components.hpp"
#include "Sources/Renderer/Components.hpp"

#include <algorithm>
#include <cmath>

namespace Game::Driving {

float laneCenter(int lane, int lane_count, float lane_width)
{
    const float center = (static_cast<float>(lane_count) - 1.0f) * 0.5f;
    return (static_cast<float>(lane) - center) * lane_width;
}

float TrafficSystem::random01()
{
    random_state_ = random_state_ * 1664525u + 1013904223u;
    return static_cast<float>((random_state_ >> 8u) & 0x00ffffffu) / 16777215.0f;
}

void TrafficSystem::update(
    Ecs::World& world,
    Ecs::Entity player,
    float delta_seconds,
    float lane_width,
    int lane_count,
    float spawn_ahead,
    float despawn_behind)
{
    Renderer::Transform *player_transform = world.get<Renderer::Transform>(player);
    Vehicle *player_vehicle = world.get<Vehicle>(player);
    if (!player_transform || !player_vehicle || lane_count <= 0) return;

    bool changed = false;

    world.each<Traffic, Renderer::Transform>(
        [&](Ecs::Entity, Traffic& traffic, Renderer::Transform& transform) {
            transform.position.z -= traffic.speed * delta_seconds;

            if (transform.position.z > player_transform->position.z + despawn_behind) {
                traffic.lane = std::clamp(
                    static_cast<int>(random01() * static_cast<float>(lane_count)),
                    0,
                    lane_count - 1
                );
                traffic.speed = traffic.heavy
                    ? 23.0f + random01() * 8.0f
                    : 27.0f + random01() * 13.0f;
                transform.position.x = laneCenter(traffic.lane, lane_count, lane_width);
                transform.position.z = player_transform->position.z - spawn_ahead - random01() * spawn_ahead;
            }

            const float half_width = traffic.heavy ? 1.25f : 0.95f;
            const float half_length = traffic.length * 0.5f;
            const float dx = std::abs(player_transform->position.x - transform.position.x);
            const float dz = std::abs(player_transform->position.z - transform.position.z);
            if (dx < half_width + 0.80f && dz < half_length + 1.60f) {
                player_vehicle->speed = std::min(player_vehicle->speed, traffic.speed) * 0.72f;
                const float push = player_transform->position.x <= transform.position.x ? -0.35f : 0.35f;
                player_transform->position.x += push;
            }

            changed = true;
        }
    );

    if (changed) world.markChanged();
}

} // namespace Game::Driving
