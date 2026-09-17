#ifndef GAME_SHOWCASE_SHOWCASE_HPP
#define GAME_SHOWCASE_SHOWCASE_HPP

#include "Discovery.hpp"

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

struct Loader
{
    std::vector<Discovery::Placement> items;
    std::size_t next = 0u;
    std::size_t loaded = 0u;
};

void prepare(const std::filesystem::path& cs2_root, Loader& loader);

bool step(
    Ecs::World& world,
    Loader& loader,
    Lineup& lineup,
    std::string *report = nullptr
);

bool complete(const Loader& loader);
void destroy(Ecs::World& world, Lineup& lineup);

} // namespace Showcase

#endif
