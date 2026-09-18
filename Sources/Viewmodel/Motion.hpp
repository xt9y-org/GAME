#ifndef GAME_VIEWMODEL_MOTION_HPP
#define GAME_VIEWMODEL_MOTION_HPP

#include <Ecs/Ecs.hpp>
#include <Renderer/Components.hpp>

namespace Viewmodel::Motion {

struct Settings
{
    Renderer::Vec3 base_position {0.0f, -0.12f, 0.0f};
    Renderer::Vec3 base_rotation {};

    float pitch_counter = 0.55f;
    float max_pitch_counter = 28.0f;

    float mouse_yaw = 0.09f;
    float mouse_pitch = 0.07f;
    float mouse_roll = 0.025f;
    float mouse_position_x = 0.0007f;
    float mouse_position_y = 0.00045f;
    float maximum_mouse_delta = 48.0f;

    float movement_position_x = 0.030f;
    float movement_position_z = 0.020f;
    float movement_roll = 1.8f;
    float movement_pitch = 1.0f;

    float bob_horizontal = 0.018f;
    float bob_vertical = 0.012f;
    float bob_roll = 1.1f;
    float walk_frequency = 7.5f;
    float sprint_frequency = 10.5f;
    float sprint_amplitude = 1.25f;

    float breathing_vertical = 0.0045f;
    float breathing_pitch = 0.20f;
    float breathing_roll = 0.18f;
    float breathing_frequency = 1.35f;

    float action_scale = 0.50f;
    float smoothing = 10.0f;
};

struct Sample
{
    float camera_pitch = 0.0f;
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    float move_x = 0.0f;
    float move_y = 0.0f;
    bool sprint = false;
    bool action_active = false;
};

struct Pose
{
    Renderer::Vec3 position {};
    Renderer::Vec3 rotation {};
};

struct State
{
    Pose pose {};
    float time = 0.0f;
    float walk_phase = 0.0f;
    bool initialized = false;
};

Pose step(
    State& state,
    const Settings& settings,
    const Sample& sample,
    float delta_seconds
);

bool apply(Ecs::World& world, Ecs::Entity entity, const Pose& pose);
void reset(State& state);

} // namespace Viewmodel::Motion

#endif
