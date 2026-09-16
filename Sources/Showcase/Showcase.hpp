#ifndef GAME_SHOWCASE_SHOWCASE_HPP
#define GAME_SHOWCASE_SHOWCASE_HPP

#include <Ecs/Ecs.hpp>
#include <Renderer/ModelScene.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Showcase {

struct Lineup
{
    std::vector<Renderer::ModelScene::Instance> instances;
    std::vector<Ecs::Entity> roots;
};

std::size_t create(
    Ecs::World& world,
    const std::filesystem::path& cs2_root,
    Lineup& lineup,
    std::string *report = nullptr
);

void destroy(Ecs::World& world, Lineup& lineup);

} // namespace Showcase

#endif
