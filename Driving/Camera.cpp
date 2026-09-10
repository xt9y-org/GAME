#include "Driving/Camera.hpp"

#include "Driving/Components.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Renderer/Components.hpp"

#include <algorithm>
#include <cmath>

namespace Game::Driving {
namespace {

constexpr float pi = 3.14159265358979323846f;

float response(float rate, float delta_seconds)
{
    return 1.0f - std::exp(-std::max(rate, 0.0f) * delta_seconds);
}

float mix(float a, float b, float amount)
{
    return a + (b - a) * amount;
}

float mixAngle(float current, float target, float amount)
{
    const float difference = std::fmod(target - current + 540.0f, 360.0f) - 180.0f;
    return current + difference * amount;
}

} // namespace

void updateCamera(Ecs::World& world, float delta_seconds)
{
    world.each<DrivingCamera, Camera::CameraComponent, Renderer::Transform>(
        [&](Ecs::Entity, DrivingCamera& camera, Camera::CameraComponent& projection, Renderer::Transform& transform) {
            const Renderer::Transform *target = world.get<Renderer::Transform>(camera.target);
            const Vehicle *vehicle = world.get<Vehicle>(camera.target);
            if (!target || !vehicle) return;

            const float speed_ratio = std::clamp(
                std::abs(vehicle->speed) / std::max(vehicle->maximum_speed, 0.001f),
                0.0f,
                1.0f
            );
            camera.vibration_phase += camera.vibration_frequency * (0.25f + 0.75f * speed_ratio) * delta_seconds;
            if (camera.vibration_phase > 2.0f * pi)
                camera.vibration_phase = std::fmod(camera.vibration_phase, 2.0f * pi);

            const float vibration = std::sin(camera.vibration_phase) * speed_ratio;
            const float secondary_vibration = std::sin(camera.vibration_phase * 0.47f + 1.3f) * speed_ratio;

            const float yaw = target->rotation.y * (pi / 180.0f);
            const float forward_x = -std::sin(yaw);
            const float forward_z = -std::cos(yaw);

            const Renderer::Vec3 desired_position {
                target->position.x + forward_x * camera.forward_offset,
                target->position.y + camera.height + vibration * camera.vibration_height,
                target->position.z + forward_z * camera.forward_offset,
            };

            const float position_mix = response(camera.position_response, delta_seconds);
            transform.position.x = mix(transform.position.x, desired_position.x, position_mix);
            transform.position.y = mix(transform.position.y, desired_position.y, position_mix);
            transform.position.z = mix(transform.position.z, desired_position.z, position_mix);

            const float pitch = camera.pitch_degrees + vehicle->brake * 1.8f - vehicle->throttle * 0.35f;
            const float target_yaw = target->rotation.y - vehicle->steering * camera.steering_look_degrees * speed_ratio;
            const float roll =
                -vehicle->steering * camera.maximum_roll_degrees * speed_ratio +
                secondary_vibration * camera.vibration_roll_degrees;
            const float rotation_mix = response(camera.rotation_response, delta_seconds);
            transform.rotation.x = mix(transform.rotation.x, pitch, rotation_mix);
            transform.rotation.y = mixAngle(transform.rotation.y, target_yaw, rotation_mix);
            transform.rotation.z = mix(transform.rotation.z, roll, rotation_mix);

            projection.fov_degrees = mix(camera.low_speed_fov, camera.high_speed_fov, speed_ratio);
        }
    );
}

} // namespace Game::Driving
