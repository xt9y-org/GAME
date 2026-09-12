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

void CameraMount::update(
    const Renderer::Transform& transform,
    float delta_seconds,
    Runtime& runtime)
{
    const float dt = std::max(delta_seconds, 0.0f);
    runtime.delta_seconds = dt;
    runtime.time_seconds += dt;

    float speed = previous_speed_;
    float acceleration = 0.0f;
    float turn_rate = 0.0f;
    if (dt > 1.0e-5f && history_) {
        const Renderer::Vec3 displacement {
            transform.position.x - previous_position_.x,
            transform.position.y - previous_position_.y,
            transform.position.z - previous_position_.z,
        };
        const float distance = length(displacement);
        const bool wrapped_or_teleported = distance > std::max(6.0f, previous_speed_ * dt * 8.0f + 2.0f);
        if (!wrapped_or_teleported) speed = distance / dt;
        turn_rate = angleDelta(transform.rotation.y, previous_yaw_) / dt;
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

    previous_position_ = transform.position;
    previous_yaw_ = transform.rotation.y;
    previous_speed_ = speed;
    history_ = true;
}

void CameraMount::reset()
{
    history_ = false;
    previous_speed_ = 0.0f;
    previous_yaw_ = 0.0f;
    previous_position_ = {};
}

} // namespace Dashcam
