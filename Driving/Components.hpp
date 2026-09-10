#ifndef GAME_DRIVING_COMPONENTS_HPP
#define GAME_DRIVING_COMPONENTS_HPP

#include "Sources/Ecs/Ecs.hpp"

namespace Game::Driving {

struct Player {};

struct Vehicle {
    float speed = 45.0f;
    float steering = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;

    float wheelbase = 2.70f;
    float maximum_speed = 83.3333f;
    float maximum_reverse_speed = 8.0f;
    float engine_acceleration = 10.5f;
    float brake_deceleration = 18.0f;
    float coast_deceleration = 0.75f;
    float aerodynamic_drag = 0.0014f;
    float maximum_steering_degrees = 31.0f;
    float steering_response = 5.0f;
    float road_half_width = 9.0f;
};

struct DrivingCamera {
    Ecs::Entity target = Ecs::INVALID_ENTITY;
    float forward_offset = 1.65f;
    float height = 0.38f;
    float pitch_degrees = -8.0f;
    float low_speed_fov = 96.0f;
    float high_speed_fov = 108.0f;
    float position_response = 28.0f;
    float rotation_response = 20.0f;
    float maximum_roll_degrees = 1.6f;
    float steering_look_degrees = 1.25f;
    float vibration_height = 0.012f;
    float vibration_roll_degrees = 0.12f;
    float vibration_frequency = 17.0f;
    float vibration_phase = 0.0f;
};

struct Traffic {
    int lane = 0;
    int target_lane = 0;
    float speed = 30.0f;
    float desired_speed = 30.0f;
    float length = 4.4f;
    float lane_change_speed = 1.8f;
    float follow_distance = 34.0f;
    float lane_change_cooldown = 0.0f;
    bool heavy = false;
};

struct RoadPiece {};

} // namespace Game::Driving

#endif
