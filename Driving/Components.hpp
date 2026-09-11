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
    float road_half_width = 0.0f;
    float road_edge_margin = 0.35f;
};

struct DrivingCamera {
    Ecs::Entity target = Ecs::INVALID_ENTITY;
    float forward_offset = 1.65f;
    float height = 0.55f;
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

struct RoadPiece {};

} // namespace Game::Driving

#endif
