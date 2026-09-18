#include "Viewmodel/Motion.hpp"

#include <cassert>
#include <cmath>

namespace {

float magnitude(float value)
{
    return std::fabs(value);
}

Viewmodel::Motion::Pose settle(
    Viewmodel::Motion::State& state,
    const Viewmodel::Motion::Settings& settings,
    const Viewmodel::Motion::Sample& sample,
    int frames = 180)
{
    Viewmodel::Motion::Pose pose{};
    for (int i = 0; i < frames; ++i)
        pose = Viewmodel::Motion::step(state, settings, sample, 1.0f / 60.0f);
    return pose;
}

void testPitchDoesNotFullyFollowCamera()
{
    Viewmodel::Motion::Settings settings{};
    settings.breathing_vertical = 0.0f;
    settings.breathing_pitch = 0.0f;
    settings.breathing_roll = 0.0f;

    Viewmodel::Motion::State state{};
    Viewmodel::Motion::Sample sample{};
    sample.camera_pitch = 60.0f;

    const auto pose = settle(state, settings, sample);
    assert(pose.rotation.x < 0.0f);
    assert(60.0f + pose.rotation.x < 60.0f);
    assert(60.0f + pose.rotation.x > 0.0f);
    assert(magnitude(pose.rotation.x) <= settings.max_pitch_counter + 0.01f);
}

void testMouseMovementCreatesTrailingSway()
{
    Viewmodel::Motion::Settings settings{};
    settings.breathing_vertical = 0.0f;
    settings.breathing_pitch = 0.0f;
    settings.breathing_roll = 0.0f;

    Viewmodel::Motion::State state{};
    Viewmodel::Motion::Sample sample{};
    sample.mouse_dx = 24.0f;
    sample.mouse_dy = -12.0f;

    const auto moved = Viewmodel::Motion::step(state, settings, sample, 1.0f / 60.0f);
    assert(magnitude(moved.rotation.y) > 0.001f);
    assert(magnitude(moved.rotation.x) > 0.001f);

    sample.mouse_dx = 0.0f;
    sample.mouse_dy = 0.0f;
    const float before = magnitude(moved.rotation.y);
    const auto returned = settle(state, settings, sample, 120);
    assert(magnitude(returned.rotation.y) < before);
}

void testWalkingAddsBobAndSwing()
{
    Viewmodel::Motion::Settings settings{};
    settings.breathing_vertical = 0.0f;
    settings.breathing_pitch = 0.0f;
    settings.breathing_roll = 0.0f;

    Viewmodel::Motion::State idle_state{};
    Viewmodel::Motion::State walk_state{};
    Viewmodel::Motion::Sample idle{};
    Viewmodel::Motion::Sample walk{};
    walk.move_y = 1.0f;

    const auto idle_pose = settle(idle_state, settings, idle, 45);
    const auto walk_pose = settle(walk_state, settings, walk, 45);

    assert(magnitude(walk_pose.position.x - idle_pose.position.x) > 0.001f ||
           magnitude(walk_pose.position.y - idle_pose.position.y) > 0.001f);
    assert(magnitude(walk_pose.rotation.z - idle_pose.rotation.z) > 0.01f);
}

void testBreathingMovesIdleWeapon()
{
    Viewmodel::Motion::Settings settings{};
    settings.smoothing = 30.0f;

    Viewmodel::Motion::State state{};
    Viewmodel::Motion::Sample sample{};

    const auto first = settle(state, settings, sample, 30);
    const auto second = settle(state, settings, sample, 60);
    assert(magnitude(second.position.y - first.position.y) > 0.0001f ||
           magnitude(second.rotation.z - first.rotation.z) > 0.001f);
}

void testWeaponActionsDampenProceduralMotion()
{
    Viewmodel::Motion::Settings settings{};
    settings.breathing_vertical = 0.0f;
    settings.breathing_pitch = 0.0f;
    settings.breathing_roll = 0.0f;

    Viewmodel::Motion::State idle_state{};
    Viewmodel::Motion::State action_state{};
    Viewmodel::Motion::Sample idle{};
    idle.mouse_dx = 20.0f;
    idle.move_x = 1.0f;

    Viewmodel::Motion::Sample action = idle;
    action.action_active = true;

    const auto idle_pose = settle(idle_state, settings, idle, 10);
    const auto action_pose = settle(action_state, settings, action, 10);

    assert(magnitude(action_pose.rotation.y) < magnitude(idle_pose.rotation.y));
    assert(magnitude(action_pose.rotation.z) < magnitude(idle_pose.rotation.z));
}

} // namespace

int main()
{
    testPitchDoesNotFullyFollowCamera();
    testMouseMovementCreatesTrailingSway();
    testWalkingAddsBobAndSwing();
    testBreathingMovesIdleWeapon();
    testWeaponActionsDampenProceduralMotion();
    return 0;
}
