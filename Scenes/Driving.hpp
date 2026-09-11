#ifndef GAME_SCENES_DRIVING_HPP
#define GAME_SCENES_DRIVING_HPP

#include "Driving/Assets.hpp"

#include "Sources/Ecs/Ecs.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace Game::Scenes {

class Driving {
public:
    const char *name() const;
    bool load(Ecs::World& world, std::string& error);
    void update(Ecs::World& world, float delta_seconds);
    Ecs::Entity camera() const;
    std::size_t triangleCount() const;
    void drawDebug(Ecs::World& world);
    void emitMetrics(const std::function<void(std::string_view, double)>& emit) const;

private:
    bool createStreet(Ecs::World& world, std::string& error);
    void recycleStreet(Ecs::World& world);

    float street_segment_length_ = 120.0f;
    int street_segment_count_ = 12;
    float road_half_width_ = 9.0f;

    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    Ecs::Entity player_ = Ecs::INVALID_ENTITY;
    std::size_t triangle_count_ = 0u;
    Game::Driving::Assets::Library assets_{};
};

} // namespace Game::Scenes

#endif
