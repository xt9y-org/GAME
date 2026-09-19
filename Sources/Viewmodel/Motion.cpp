#include "Motion.hpp"

#include <algorithm>
#include <cmath>

namespace Viewmodel::Motion {
namespace {

float clampMouse(float value, float maximum)
{
    const float limit = std::max(maximum, 0.0f);
    return std::clamp(value, -limit, limit);
}

float smoothFactor(float speed, float delta_seconds)
{
    if (delta_seconds <= 0.0f) return 0.0f;
    return 1.0f - std::exp(-std::max(speed, 0.0f) * delta_seconds);
}

float approach(float current, float target, float factor)
{
    return current + (target - current) * factor;
}

void approach(Renderer::Vec3& current, const Renderer::Vec3& target, float factor)
{
    current.x = approach(current.x, target.x, factor);
    current.y = approach(current.y, target.y, factor);
    current.z = approach(current.z, target.z, factor);
}

} // namespace

Pose step(
    State& state,
    const Settings& settings,
    const Sample& sample,
    float delta_seconds)
{
    const float delta = std::clamp(delta_seconds, 0.0f, 0.1f);
    state.time += delta;

    const float move_x = std::clamp(sample.move_x, -1.0f, 1.0f);
    const float move_y = std::clamp(sample.move_y, -1.0f, 1.0f);
    const float movement = std::clamp(
        std::sqrt(move_x * move_x + move_y * move_y),
        0.0f,
        1.0f
    );

    if (movement > 0.001f) {
        const float frequency = sample.sprint
            ? settings.sprint_frequency
            : settings.walk_frequency;
        state.walk_phase += delta * std::max(frequency, 0.0f);
    }

    const float action = sample.action_active
        ? std::clamp(settings.action_scale, 0.0f, 1.0f)
        : 1.0f;
    const float sprint = sample.sprint
        ? std::max(settings.sprint_amplitude, 0.0f)
        : 1.0f;
    const float dynamic = action * sprint;

    const float mouse_dx = clampMouse(sample.mouse_dx, settings.maximum_mouse_delta);
    const float mouse_dy = clampMouse(sample.mouse_dy, settings.maximum_mouse_delta);

    Pose target;
    target.position = settings.base_position;
    target.rotation = settings.base_rotation;

    target.rotation.x += std::clamp(
        -sample.camera_pitch * settings.pitch_counter,
        -std::max(settings.max_pitch_counter, 0.0f),
        std::max(settings.max_pitch_counter, 0.0f)
    );

    target.rotation.y += mouse_dx * settings.mouse_yaw * action;
    target.rotation.x += mouse_dy * settings.mouse_pitch * action;
    target.rotation.z += mouse_dx * settings.mouse_roll * action;
    target.position.x += mouse_dx * settings.mouse_position_x * action;
    target.position.y += mouse_dy * settings.mouse_position_y * action;

    target.position.x -= move_x * settings.movement_position_x * dynamic;
    target.position.z += move_y * settings.movement_position_z * dynamic;
    target.rotation.z -= move_x * settings.movement_roll * dynamic;
    target.rotation.x += move_y * settings.movement_pitch * dynamic;

    if (movement > 0.001f) {
        const float horizontal = std::sin(state.walk_phase);
        const float vertical = std::sin(state.walk_phase * 2.0f);
        target.position.x += horizontal * settings.bob_horizontal * movement * dynamic;
        target.position.y += vertical * settings.bob_vertical * movement * dynamic;
        target.rotation.z += horizontal * settings.bob_roll * movement * dynamic;
    }

    const float breath = std::sin(state.time * settings.breathing_frequency);
    const float breath_quadrature = std::sin(
        state.time * settings.breathing_frequency + 1.57079632679f
    );
    target.position.y += breath * settings.breathing_vertical * action;
    target.rotation.x += breath_quadrature * settings.breathing_pitch * action;
    target.rotation.z += breath * settings.breathing_roll * action;

    if (!state.initialized) {
        state.pose.position = settings.base_position;
        state.pose.rotation = settings.base_rotation;
        state.initialized = true;
    }

    const float factor = smoothFactor(settings.smoothing, delta);
    approach(state.pose.position, target.position, factor);
    approach(state.pose.rotation, target.rotation, factor);
    return state.pose;
}

bool apply(Ecs::World& world, Ecs::Entity entity, const Pose& pose)
{
    Renderer::Transform *transform = world.get<Renderer::Transform>(entity);
    if (!transform) return false;

    transform->position = pose.position;
    transform->rotation = pose.rotation;
    transform->matrix_override_enabled = false;
    world.markChanged(Ecs::ChangeKind::Transform, entity);
    return true;
}

void reset(State& state)
{
    state = {};
}

} // namespace Viewmodel::Motion
