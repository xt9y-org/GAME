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
};

struct Traffic {
    int lane = 0;
    float speed = 30.0f;
    float length = 4.4f;
    bool heavy = false;
};

struct RoadPiece {};

} // namespace Game::Driving

#endif
