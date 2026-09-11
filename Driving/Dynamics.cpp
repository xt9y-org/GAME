#include "Driving/Dynamics.hpp"

#include "Driving/Components.hpp"
#include "Sources/Renderer/Components.hpp"

#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <cmath>

namespace Game::Driving {
namespace {

constexpr float pi = 3.14159265358979323846f;

float moveTowards(float current, float target, float maximum_delta)
{
    if (current < target) return std::min(current + maximum_delta, target);
    return std::max(current - maximum_delta, target);
}

} // namespace

void updatePlayer(Ecs::World& world, float delta_seconds, bool input_enabled)
{
    bool changed = false;

    world.each<Player, Vehicle, Renderer::Transform>(
        [&](Ecs::Entity, Player&, Vehicle& vehicle, Renderer::Transform& transform) {
            const float left = input_enabled && Keyboard.isKeyDown(Keyboard.KEY_A) ? 1.0f : 0.0f;
            const float right = input_enabled && Keyboard.isKeyDown(Keyboard.KEY_D) ? 1.0f : 0.0f;
            vehicle.throttle = input_enabled && Keyboard.isKeyDown(Keyboard.KEY_W) ? 1.0f : 0.0f;
            vehicle.brake = input_enabled && Keyboard.isKeyDown(Keyboard.KEY_S) ? 1.0f : 0.0f;

            const float steering_target = left - right;
            vehicle.steering = moveTowards(
                vehicle.steering,
                steering_target,
                vehicle.steering_response * delta_seconds
            );

            if (vehicle.throttle > 0.0f) {
                vehicle.speed += vehicle.engine_acceleration * vehicle.throttle * delta_seconds;
            } else if (vehicle.brake > 0.0f) {
                if (vehicle.speed > 0.0f) {
                    vehicle.speed -= vehicle.brake_deceleration * vehicle.brake * delta_seconds;
                } else {
                    vehicle.speed -= vehicle.engine_acceleration * 0.35f * vehicle.brake * delta_seconds;
                }
            } else {
                vehicle.speed = moveTowards(vehicle.speed, 0.0f, vehicle.coast_deceleration * delta_seconds);
            }

            const float drag = vehicle.aerodynamic_drag * vehicle.speed * std::abs(vehicle.speed);
            vehicle.speed -= drag * delta_seconds;
            vehicle.speed = std::clamp(
                vehicle.speed,
                -vehicle.maximum_reverse_speed,
                vehicle.maximum_speed
            );

            const float speed_ratio = std::clamp(
                std::abs(vehicle.speed) / std::max(vehicle.maximum_speed, 0.001f),
                0.0f,
                1.0f
            );
            const float steering_limit = vehicle.maximum_steering_degrees * (1.0f - 0.72f * speed_ratio);
            const float steering_radians = vehicle.steering * steering_limit * (pi / 180.0f);
            const float yaw_rate_radians =
                std::abs(vehicle.wheelbase) > 0.001f
                    ? (vehicle.speed / vehicle.wheelbase) * std::tan(steering_radians)
                    : 0.0f;
            transform.rotation.y += yaw_rate_radians * (180.0f / pi) * delta_seconds;

            const float yaw = transform.rotation.y * (pi / 180.0f);
            const float forward_x = -std::sin(yaw);
            const float forward_z = -std::cos(yaw);
            transform.position.x += forward_x * vehicle.speed * delta_seconds;
            transform.position.z += forward_z * vehicle.speed * delta_seconds;

            changed = true;
        }
    );

    if (changed) world.markChanged(Ecs::ChangeKind::Transform);
}

} // namespace Game::Driving
