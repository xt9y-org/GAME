#include "Dashcam/CameraMount.hpp"

#include <algorithm>
#include <cmath>

namespace Dashcam {
namespace {

constexpr float pi = 3.14159265358979323846f;

float length(const Renderer::Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

float angleDelta(float current, float previous)
{
    float delta = std::fmod(current - previous, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return delta;
}

} // namespace

void CameraMount::beginFrame(Renderer::Transform& transform)
{
    if (!applied_) return;
    transform = base_;
    applied_ = false;
}

void CameraMount::update(
    Renderer::Transform& transform,
    float delta_seconds,
    const Settings& settings,
    Runtime& runtime)
{
    base_ = transform;
    const float dt = std::max(delta_seconds, 0.0f);
    runtime.delta_seconds = dt;
    runtime.time_seconds += dt;

    float speed = previous_speed_;
    float acceleration = 0.0f;
    float turn_rate = 0.0f;
    if (dt > 1.0e-5f && history_) {
        const Renderer::Vec3 displacement {
            base_.position.x - previous_position_.x,
            base_.position.y - previous_position_.y,
            base_.position.z - previous_position_.z,
        };
        const float distance = length(displacement);
        const bool wrapped_or_teleported = distance > std::max(6.0f, previous_speed_ * dt * 8.0f + 2.0f);
        if (!wrapped_or_teleported) speed = distance / dt;
        turn_rate = angleDelta(base_.rotation.y, previous_yaw_) / dt;
        acceleration = (speed - previous_speed_) / dt;
    } else if (dt > 1.0e-5f) {
        speed = 0.0f;
    }

    const float yaw_rate_radians = turn_rate * (pi / 180.0f);
    const float lateral_acceleration = speed * yaw_rate_radians;
    const float total_acceleration = std::sqrt(
        acceleration * acceleration + lateral_acceleration * lateral_acceleration
    );

    runtime.speed = speed;
    runtime.acceleration = acceleration;
    runtime.turn_rate = turn_rate;
    runtime.g_force = std::clamp(total_acceleration / 9.80665f, 0.0f, 4.0f);

    previous_position_ = base_.position;
    previous_yaw_ = base_.rotation.y;
    previous_speed_ = speed;
    history_ = true;

    if (!settings.enabled) return;

    const float vibration = settings.vibration * (0.15f + std::min(speed / 35.0f, 1.0f));
    const float wave_a = std::sin(runtime.time_seconds * 83.0f);
    const float wave_b = std::sin(runtime.time_seconds * 127.0f + 1.7f);
    const float wave_c = std::sin(runtime.time_seconds * 59.0f + 0.4f);
    const float inertia = settings.inertia;

    transform.rotation.x +=
        std::clamp(-acceleration * inertia * 0.030f, -2.6f, 2.6f) +
        wave_a * vibration * 0.18f;
    transform.rotation.y += wave_b * vibration * 0.09f;
    transform.rotation.z +=
        std::clamp(-turn_rate * inertia * 0.010f, -3.2f, 3.2f) +
        wave_c * vibration * 0.22f;

    const float shake = vibration * 0.0025f;
    transform.position.x += wave_b * shake;
    transform.position.y += wave_a * shake * 0.7f;
    transform.position.z += wave_c * shake * 0.45f;
    applied_ = true;
}

void CameraMount::reset()
{
    applied_ = false;
    history_ = false;
    previous_speed_ = 0.0f;
    previous_yaw_ = 0.0f;
    previous_position_ = {};
}

} // namespace Dashcam
