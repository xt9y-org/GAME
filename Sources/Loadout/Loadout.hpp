#ifndef GAME_LOADOUT_LOADOUT_HPP
#define GAME_LOADOUT_LOADOUT_HPP

#include <Ecs/Ecs.hpp>
#include <Renderer/ModelScene.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Loadout {

struct Item
{
    std::string name;
    std::filesystem::path path;
};

struct State
{
    std::filesystem::path root;
    Ecs::Entity parent = Ecs::INVALID_ENTITY;

    std::vector<Item> weapons;
    std::vector<Item> arms;
    std::size_t weapon = 0u;
    std::size_t arm = 0u;

    std::unique_ptr<Renderer::ModelScene::Instance> weapon_instance;
    std::unique_ptr<Renderer::ModelScene::Instance> arm_instance;
    Renderer::ModelScene::Animation animation;

    Models::ModelHandle idle = Models::INVALID_MODEL;
    Models::ModelHandle shoot = Models::INVALID_MODEL;
    Models::ModelHandle reload = Models::INVALID_MODEL;
    Models::ModelHandle inspect = Models::INVALID_MODEL;

    std::string error;
};

bool init(
    State& state,
    Ecs::World& world,
    Ecs::Entity parent,
    const std::filesystem::path& root,
    std::string *error = nullptr
);

bool selectWeapon(State& state, Ecs::World& world, std::size_t index);
bool selectArms(State& state, Ecs::World& world, std::size_t index);

bool shoot(State& state, std::string *error = nullptr);
bool reload(State& state, std::string *error = nullptr);
bool inspect(State& state, std::string *error = nullptr);
bool update(State& state, Ecs::World& world, float delta_seconds, std::string *error = nullptr);

void destroy(State& state, Ecs::World& world);

} // namespace Loadout

#endif
