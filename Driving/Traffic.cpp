#include "Driving/Traffic.hpp"

#include "Driving/Components.hpp"
#include "Sources/Renderer/Components.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Game::Driving {
namespace {

float moveTowards(float current, float target, float maximum_delta)
{
    if (current < target) return std::min(current + maximum_delta, target);
    return std::max(current - maximum_delta, target);
}

struct Entry {
    Traffic *traffic = nullptr;
    Renderer::Transform *transform = nullptr;
};

bool laneClear(
    const std::vector<Entry>& entries,
    const Entry& subject,
    int lane,
    float rear_gap,
    float front_gap)
{
    for (const Entry& entry : entries) {
        if (&entry == &subject || !entry.traffic || !entry.transform) continue;
        if (entry.traffic->target_lane != lane && entry.traffic->lane != lane) continue;

        const float dz = subject.transform->position.z - entry.transform->position.z;
        if (dz >= 0.0f && dz < front_gap) return false;
        if (dz < 0.0f && -dz < rear_gap) return false;
    }
    return true;
}

} // namespace

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

    std::vector<Entry> entries;
    world.each<Traffic, Renderer::Transform>(
        [&](Ecs::Entity, Traffic& traffic, Renderer::Transform& transform) {
            entries.push_back(Entry{&traffic, &transform});
        }
    );

    statistics_.count = entries.size();
    statistics_.average_speed = 0.0f;
    statistics_.nearest_ahead = std::numeric_limits<float>::infinity();

    for (Entry& entry : entries) {
        Traffic& traffic = *entry.traffic;
        Renderer::Transform& transform = *entry.transform;

        traffic.lane_change_cooldown = std::max(
            traffic.lane_change_cooldown - delta_seconds,
            0.0f
        );

        const Entry *nearest = nullptr;
        float nearest_distance = std::numeric_limits<float>::infinity();
        for (const Entry& other : entries) {
            if (&other == &entry || !other.traffic || !other.transform) continue;
            if (other.traffic->lane != traffic.lane) continue;

            const float distance = transform.position.z - other.transform->position.z;
            if (distance > 0.0f && distance < nearest_distance) {
                nearest = &other;
                nearest_distance = distance;
            }
        }

        if (traffic.target_lane == traffic.lane && nearest &&
            nearest_distance < traffic.follow_distance && traffic.lane_change_cooldown <= 0.0f)
        {
            const int preferred = random01() < 0.5f ? -1 : 1;
            const int candidates[2] = {traffic.lane + preferred, traffic.lane - preferred};
            for (const int candidate : candidates) {
                if (candidate < 0 || candidate >= lane_count) continue;
                if (!laneClear(entries, entry, candidate, 18.0f, 30.0f)) continue;
                traffic.target_lane = candidate;
                traffic.lane_change_cooldown = 2.0f + random01() * 2.5f;
                ++statistics_.lane_changes;
                break;
            }
        }

        float target_speed = traffic.desired_speed;
        if (nearest && nearest_distance < traffic.follow_distance * 1.6f) {
            const float ratio = std::clamp(
                nearest_distance / std::max(traffic.follow_distance, 0.01f),
                0.0f,
                1.0f
            );
            target_speed = std::min(
                target_speed,
                nearest->traffic->speed * (0.62f + 0.38f * ratio)
            );
        }

        const float acceleration = target_speed >= traffic.speed ? 2.6f : 6.5f;
        traffic.speed = moveTowards(
            traffic.speed,
            std::max(target_speed, 0.0f),
            acceleration * delta_seconds
        );
        transform.position.z -= traffic.speed * delta_seconds;

        const float target_x = laneCenter(traffic.target_lane, lane_count, lane_width);
        transform.position.x = moveTowards(
            transform.position.x,
            target_x,
            traffic.lane_change_speed * delta_seconds
        );
        if (std::abs(transform.position.x - target_x) < 0.02f) {
            transform.position.x = target_x;
            traffic.lane = traffic.target_lane;
        }

        if (transform.position.z > player_transform->position.z + despawn_behind) {
            traffic.lane = std::clamp(
                static_cast<int>(random01() * static_cast<float>(lane_count)),
                0,
                lane_count - 1
            );
            traffic.target_lane = traffic.lane;
            traffic.desired_speed = traffic.heavy
                ? 23.0f + random01() * 8.0f
                : 27.0f + random01() * 13.0f;
            traffic.speed = traffic.desired_speed;
            traffic.lane_change_cooldown = 1.0f + random01() * 3.0f;
            transform.position.x = laneCenter(traffic.lane, lane_count, lane_width);
            transform.position.z = player_transform->position.z - spawn_ahead - random01() * spawn_ahead;
        }

        const float player_ahead_distance = player_transform->position.z - transform.position.z;
        if (player_ahead_distance > 0.0f)
            statistics_.nearest_ahead = std::min(statistics_.nearest_ahead, player_ahead_distance);
        statistics_.average_speed += traffic.speed;

        const float half_width = traffic.heavy ? 1.25f : 0.95f;
        const float half_length = traffic.length * 0.5f;
        const float dx = std::abs(player_transform->position.x - transform.position.x);
        const float dz = std::abs(player_transform->position.z - transform.position.z);
        if (dx < half_width + 0.80f && dz < half_length + 1.60f) {
            player_vehicle->speed = std::min(player_vehicle->speed, traffic.speed) * 0.72f;
            const float push = player_transform->position.x <= transform.position.x ? -0.35f : 0.35f;
            player_transform->position.x += push;
        }
    }

    if (!entries.empty())
        statistics_.average_speed /= static_cast<float>(entries.size());
    if (!std::isfinite(statistics_.nearest_ahead)) statistics_.nearest_ahead = 0.0f;

    if (!entries.empty()) world.markChanged(Ecs::ChangeKind::Transform);
}

} // namespace Game::Driving
