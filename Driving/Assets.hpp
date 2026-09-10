#ifndef GAME_DRIVING_ASSETS_HPP
#define GAME_DRIVING_ASSETS_HPP

#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Components.hpp"

#include <cstddef>
#include <vector>

namespace Game::Driving::Assets {

struct Model {
    Models::ModelHandle handle = Models::INVALID_MODEL;
    Models::Bounds bounds{};
    float yaw_degrees = 0.0f;

    bool valid() const { return handle != Models::INVALID_MODEL; }
};

struct Library {
    std::vector<Model> cars;
    std::vector<Model> heavy_traffic;
    std::vector<Model> street;
    std::vector<Model> foliage;
    std::vector<Model> city;

    std::size_t requested = 0u;
    std::size_t files_present = 0u;
    std::size_t loaded = 0u;

    void load();
};

std::size_t attach(
    Ecs::World& world,
    Ecs::Entity parent,
    const Model& model,
    float target_length,
    float yaw_offset_degrees = 0.0f
);

} // namespace Game::Driving::Assets

#endif
