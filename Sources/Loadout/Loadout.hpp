#ifndef GAME_LOADOUT_LOADOUT_HPP
#define GAME_LOADOUT_LOADOUT_HPP

#include <Ecs/Ecs.hpp>
#include <Renderer/ModelScene.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Loadout {

enum class WeaponCategory : std::uint8_t
{
    Pistols,
    Smgs,
    Rifles,
    Snipers,
    Shotguns,
    MachineGuns,
};

struct Item
{
    std::string name;
    std::filesystem::path path;
};

struct WeaponItem
{
    std::string name;
    WeaponCategory category = WeaponCategory::Pistols;
    std::filesystem::path path;
    std::filesystem::path idle;
    std::filesystem::path shoot;
    std::filesystem::path reload;
    std::filesystem::path inspect;
};

struct State
{
    std::filesystem::path root;
    Ecs::Entity parent = Ecs::INVALID_ENTITY;

    std::vector<WeaponItem> weapons;
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

    bool shooting = false;
    bool action_active = false;
    std::string error;
};

const char *categoryName(WeaponCategory category);

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
