#ifndef GAME_DRIVING_TRAFFIC_HPP
#define GAME_DRIVING_TRAFFIC_HPP

#include "Sources/Ecs/Ecs.hpp"

#include <cstddef>
#include <cstdint>

namespace Game::Driving {

class TrafficSystem {
public:
    struct Statistics {
        std::size_t count = 0u;
        float average_speed = 0.0f;
        float nearest_ahead = 0.0f;
        std::uint64_t lane_changes = 0u;
    };

    void setSeed(std::uint32_t seed)
    {
        random_state_ = seed ? seed : 1u;
        statistics_ = {};
    }

    void update(
        Ecs::World& world,
        Ecs::Entity player,
        float delta_seconds,
        float lane_width,
        int lane_count,
        float spawn_ahead,
        float despawn_behind
    );

    const Statistics& statistics() const { return statistics_; }

private:
    float random01();

    std::uint32_t random_state_ = 0x12345678u;
    Statistics statistics_{};
};

float laneCenter(int lane, int lane_count, float lane_width);

} // namespace Game::Driving

#endif
