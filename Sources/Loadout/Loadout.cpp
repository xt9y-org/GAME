#include "Loadout.hpp"

#include <Models/Models.hpp>
#include <Models/Runtime.hpp>

#include <algorithm>
#include <system_error>
#include <utility>

namespace Loadout {
namespace {

constexpr const char *DefaultArms =
    "Arms/agents/models/shared/arms/glove_fullfinger/glove_fullfinger.gltf";
constexpr const char *DefaultWeapon =
    "Models/weapons/models/revolver/weapon_pist_revolver.gltf";
constexpr const char *IdleAnimation =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/idle_revolver.gltf";
constexpr const char *ShootAnimation =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/shoot1_revolver.gltf";
constexpr const char *ReloadAnimation =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/reload_revolver.gltf";
constexpr const char *InspectAnimation =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/lookat01_revolver.gltf";

bool modelFile(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    return extension == ".gltf" || extension == ".glb";
}

std::string label(const std::filesystem::path& path)
{
    std::string value = path.stem().string();
    constexpr const char *prefixes[] = {"weapon_pist_", "weapon_"};
    for (const char *prefix : prefixes) {
        const std::size_t size = std::char_traits<char>::length(prefix);
        if (value.rfind(prefix, 0u) == 0u) {
            value.erase(0u, size);
            break;
        }
    }
    std::replace(value.begin(), value.end(), '_', ' ');
    return value;
}

std::vector<Item> discover(const std::filesystem::path& root, bool weapons)
{
    std::vector<Item> items;
    std::error_code error;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::skip_permission_denied,
             error),
         end;
         !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error) || !modelFile(it->path())) continue;
        if (weapons && it->path().filename().string().rfind("weapon_", 0u) != 0u) continue;
        items.push_back(Item{label(it->path()), it->path().lexically_normal()});
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.path.string() < b.path.string();
    });
    items.erase(
        std::unique(items.begin(), items.end(), [](const Item& a, const Item& b) {
            return a.path == b.path;
        }),
        items.end()
    );
    return items;
}

std::size_t ensure(
    std::vector<Item>& items,
    const std::filesystem::path& path)
{
    const std::filesystem::path normalized = path.lexically_normal();
    for (std::size_t index = 0u; index < items.size(); ++index)
        if (items[index].path == normalized) return index;

    items.push_back(Item{label(normalized), normalized});
    return items.size() - 1u;
}

bool fail(State& state, const std::string& message, std::string *error = nullptr)
{
    state.error = message;
    if (error) *error = message;
    return false;
}

Models::ModelHandle load(
    State& state,
    const std::filesystem::path& path,
    std::string *error)
{
    std::string message;
    const Models::ModelHandle model = Models::load(path.string(), &message);
    if (model == Models::INVALID_MODEL) {
        fail(
            state,
            message.empty() ? "Could not load " + path.string() : message,
            error
        );
    }
    return model;
}

Renderer::ModelScene::Options options(const State& state)
{
    return Renderer::ModelScene::Options{
        .parent = state.parent,
        .instantiate_cameras = false,
        .instantiate_lights = false,
    };
}

bool bind(
    State& state,
    Renderer::ModelScene::Animation& animation,
    Renderer::ModelScene::Instance& arms,
    Renderer::ModelScene::Instance& weapon,
    std::string *error)
{
    const std::size_t arms_target = Renderer::ModelScene::bind(
        animation,
        arms,
        Models::Runtime::RetargetOptions{
            .mode = Models::Runtime::RetargetMode::World,
            .source_root = "root_motion",
        }
    );
    const std::size_t weapon_target = Renderer::ModelScene::bind(animation, weapon);

    if (arms_target == Models::INVALID_INDEX ||
        weapon_target == Models::INVALID_INDEX ||
        !Renderer::ModelScene::attach(animation, weapon_target, "wpn", "weapon"))
        return fail(state, "Could not bind viewmodel loadout", error);

    std::string message;
    if (!Renderer::ModelScene::play(animation, state.idle, 0u, true, &message))
        return fail(state, message.empty() ? "Could not play viewmodel idle animation" : message, error);

    return true;
}

bool play(State& state, Models::ModelHandle animation, std::string *error)
{
    std::string message;
    if (!Renderer::ModelScene::play(state.animation, animation, 0u, false, &message))
        return fail(state, message.empty() ? "Could not play viewmodel animation" : message, error);
    state.error.clear();
    if (error) error->clear();
    return true;
}

} // namespace

bool init(
    State& state,
    Ecs::World& world,
    Ecs::Entity parent,
    const std::filesystem::path& root,
    std::string *error)
{
    destroy(state, world);
    state.root = root.lexically_normal();
    state.parent = parent;

    const std::filesystem::path weapon_path = state.root / DefaultWeapon;
    const std::filesystem::path arm_path = state.root / DefaultArms;

    state.weapons = discover(state.root / "Models/weapons/models", true);
    state.arms = discover(state.root / "Arms/agents/models/shared/arms", false);
    state.weapon = ensure(state.weapons, weapon_path);
    state.arm = ensure(state.arms, arm_path);

    const Models::ModelHandle arms = load(state, arm_path, error);
    if (arms == Models::INVALID_MODEL) return false;

    const Models::ModelHandle weapon = load(state, weapon_path, error);
    if (weapon == Models::INVALID_MODEL) return false;

    state.idle = load(state, state.root / IdleAnimation, error);
    if (state.idle == Models::INVALID_MODEL) return false;
    state.shoot = load(state, state.root / ShootAnimation, error);
    if (state.shoot == Models::INVALID_MODEL) return false;
    state.reload = load(state, state.root / ReloadAnimation, error);
    if (state.reload == Models::INVALID_MODEL) return false;
    state.inspect = load(state, state.root / InspectAnimation, error);
    if (state.inspect == Models::INVALID_MODEL) return false;

    state.arm_instance = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            arms,
            state.arm_instance.get(),
            options(state),
            error))
        return fail(state, error && !error->empty() ? *error : "Could not instantiate viewmodel arms", error);

    state.weapon_instance = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            weapon,
            state.weapon_instance.get(),
            options(state),
            error))
        return fail(state, error && !error->empty() ? *error : "Could not instantiate viewmodel weapon", error);

    Renderer::ModelScene::Animation animation;
    if (!bind(
            state,
            animation,
            *state.arm_instance,
            *state.weapon_instance,
            error))
        return false;

    state.animation = std::move(animation);
    state.error.clear();
    if (error) error->clear();
    return true;
}

bool selectWeapon(State& state, Ecs::World& world, std::size_t index)
{
    if (index >= state.weapons.size())
        return fail(state, "Invalid weapon selection");
    if (index == state.weapon) return true;

    std::string message;
    const Models::ModelHandle model = load(state, state.weapons[index].path, &message);
    if (model == Models::INVALID_MODEL) return false;

    auto replacement = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            model,
            replacement.get(),
            options(state),
            &message))
        return fail(state, message.empty() ? "Could not instantiate selected weapon" : message);

    Renderer::ModelScene::Animation animation;
    if (!bind(state, animation, *state.arm_instance, *replacement, &message)) {
        Renderer::ModelScene::destroy(world, *replacement);
        return false;
    }

    Renderer::ModelScene::destroy(world, *state.weapon_instance);
    state.weapon_instance = std::move(replacement);
    state.weapon = index;
    state.animation = std::move(animation);
    state.error.clear();
    return true;
}

bool selectArms(State& state, Ecs::World& world, std::size_t index)
{
    if (index >= state.arms.size())
        return fail(state, "Invalid arms selection");
    if (index == state.arm) return true;

    std::string message;
    const Models::ModelHandle model = load(state, state.arms[index].path, &message);
    if (model == Models::INVALID_MODEL) return false;

    auto replacement = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            model,
            replacement.get(),
            options(state),
            &message))
        return fail(state, message.empty() ? "Could not instantiate selected arms" : message);

    Renderer::ModelScene::Animation animation;
    if (!bind(state, animation, *replacement, *state.weapon_instance, &message)) {
        Renderer::ModelScene::destroy(world, *replacement);
        return false;
    }

    Renderer::ModelScene::destroy(world, *state.arm_instance);
    state.arm_instance = std::move(replacement);
    state.arm = index;
    state.animation = std::move(animation);
    state.error.clear();
    return true;
}

bool shoot(State& state, std::string *error)
{
    return play(state, state.shoot, error);
}

bool reload(State& state, std::string *error)
{
    return play(state, state.reload, error);
}

bool inspect(State& state, std::string *error)
{
    return play(state, state.inspect, error);
}

bool update(State& state, Ecs::World& world, float delta_seconds, std::string *error)
{
    std::string message;
    if (!Renderer::ModelScene::update(world, state.animation, delta_seconds, &message))
        return fail(state, message.empty() ? "Could not update viewmodel animation" : message, error);

    if (!Renderer::ModelScene::playing(state.animation) &&
        !Renderer::ModelScene::play(state.animation, state.idle, 0u, true, &message))
        return fail(state, message.empty() ? "Could not restart viewmodel idle animation" : message, error);

    state.error.clear();
    if (error) error->clear();
    return true;
}

void destroy(State& state, Ecs::World& world)
{
    state.animation = {};

    if (state.weapon_instance) {
        Renderer::ModelScene::destroy(world, *state.weapon_instance);
        state.weapon_instance.reset();
    }
    if (state.arm_instance) {
        Renderer::ModelScene::destroy(world, *state.arm_instance);
        state.arm_instance.reset();
    }

    state.weapons.clear();
    state.arms.clear();
    state.idle = Models::INVALID_MODEL;
    state.shoot = Models::INVALID_MODEL;
    state.reload = Models::INVALID_MODEL;
    state.inspect = Models::INVALID_MODEL;
    state.error.clear();
}

} // namespace Loadout
