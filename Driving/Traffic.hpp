#ifndef GAME_DRIVING_TRAFFIC_HPP
#define GAME_DRIVING_TRAFFIC_HPP

#include "Sources/Ecs/Ecs.hpp"

#include <cstdint>

namespace Game::Driving {

class TrafficSystem {
public:
    void setSeed(std::uint32_t seed) { random_state_ = seed ? seed : 1u; }
    void update(
        Ecs::World& world,
        Ecs::Entity player,
        float delta_seconds,
        float lane_width,
        int lane_count,
        float spawn_ahead,
        float despawn_behind
    );

private:
    float random01();
    std::uint32_t random_state_ = 0x12345678u;
};

float laneCenter(int lane, int lane_count, float lane_width);

} // namespace Game::Driving

#endif
