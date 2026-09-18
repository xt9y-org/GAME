#include "Loadout.hpp"

#include <Models/Models.hpp>
#include <Models/Runtime.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace Loadout {
namespace {

constexpr const char *DefaultArms =
    "Arms/agents/models/shared/arms/glove_fullfinger/glove_fullfinger.gltf";
constexpr const char *DefaultWeapon =
    "Models/weapons/models/revolver/weapon_pist_revolver.gltf";
constexpr const char *DefaultIdle =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/idle_revolver.gltf";
constexpr const char *DefaultShoot =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/shoot1_revolver.gltf";
constexpr const char *DefaultReload =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/reload_revolver.gltf";
constexpr const char *DefaultInspect =
    "Anim/animation/anims/viewmodel/pistol/pistol_revolver/lookat01_revolver.gltf";

struct WeaponSpec
{
    const char *name;
    WeaponCategory category;
    const char *token;
    const char *alternate;
    const char *exclude;
};

constexpr std::array<WeaponSpec, 34> WeaponSpecs{{
    {"Glock-18",       WeaponCategory::Pistols,     "glock",          nullptr,       nullptr},
    {"P2000",          WeaponCategory::Pistols,     "hkp2000",        "p2000",       nullptr},
    {"USP-S",          WeaponCategory::Pistols,     "usp_silencer",   "usp",         nullptr},
    {"Dual Berettas",  WeaponCategory::Pistols,     "elite",          "dual",        nullptr},
    {"P250",           WeaponCategory::Pistols,     "p250",           nullptr,       nullptr},
    {"Five-SeveN",     WeaponCategory::Pistols,     "fiveseven",      "five_seven",  nullptr},
    {"Tec-9",          WeaponCategory::Pistols,     "tec9",           "tec_9",       nullptr},
    {"CZ75-Auto",      WeaponCategory::Pistols,     "cz75a",          "cz75",        nullptr},
    {"Desert Eagle",   WeaponCategory::Pistols,     "deagle",         nullptr,       nullptr},
    {"R8 Revolver",    WeaponCategory::Pistols,     "revolver",       nullptr,       nullptr},

    {"MAC-10",         WeaponCategory::Smgs,        "mac10",          "mac_10",      nullptr},
    {"MP9",            WeaponCategory::Smgs,        "mp9",            nullptr,       nullptr},
    {"MP7",            WeaponCategory::Smgs,        "mp7",            nullptr,       nullptr},
    {"MP5-SD",         WeaponCategory::Smgs,        "mp5sd",          "mp5",         nullptr},
    {"UMP-45",         WeaponCategory::Smgs,        "ump45",          "ump",         nullptr},
    {"P90",            WeaponCategory::Smgs,        "p90",            nullptr,       nullptr},
    {"PP-Bizon",       WeaponCategory::Smgs,        "bizon",          nullptr,       nullptr},

    {"Galil AR",       WeaponCategory::Rifles,      "galilar",        "galil",       nullptr},
    {"FAMAS",          WeaponCategory::Rifles,      "famas",          nullptr,       nullptr},
    {"AK-47",          WeaponCategory::Rifles,      "ak47",           "ak_47",       nullptr},
    {"M4A4",           WeaponCategory::Rifles,      "m4a1",           "m4a4",        "silencer"},
    {"M4A1-S",         WeaponCategory::Rifles,      "m4a1_silencer",  "m4a1s",       nullptr},
    {"AUG",            WeaponCategory::Rifles,      "aug",            nullptr,       nullptr},
    {"SG 553",         WeaponCategory::Rifles,      "sg556",          "sg553",       nullptr},

    {"SSG 08",         WeaponCategory::Snipers,     "ssg08",          "scout",       nullptr},
    {"AWP",            WeaponCategory::Snipers,     "awp",            nullptr,       nullptr},
    {"SCAR-20",        WeaponCategory::Snipers,     "scar20",         "scar_20",     nullptr},
    {"G3SG1",          WeaponCategory::Snipers,     "g3sg1",          nullptr,       nullptr},

    {"Nova",           WeaponCategory::Shotguns,    "nova",           nullptr,       nullptr},
    {"XM1014",         WeaponCategory::Shotguns,    "xm1014",         nullptr,       nullptr},
    {"MAG-7",          WeaponCategory::Shotguns,    "mag7",           "mag_7",       nullptr},
    {"Sawed-Off",      WeaponCategory::Shotguns,    "sawedoff",       "sawed_off",   nullptr},

    {"M249",           WeaponCategory::MachineGuns, "m249",           nullptr,       nullptr},
    {"Negev",          WeaponCategory::MachineGuns, "negev",          nullptr,       nullptr},
}};

enum class Action
{
    Idle,
    Shoot,
    Reload,
    Inspect,
};

struct AnimationHandles
{
    Models::ModelHandle idle = Models::INVALID_MODEL;
    Models::ModelHandle shoot = Models::INVALID_MODEL;
    Models::ModelHandle reload = Models::INVALID_MODEL;
    Models::ModelHandle inspect = Models::INVALID_MODEL;
};

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string pathText(const std::filesystem::path& path)
{
    return lower(path.generic_string());
}

bool modelFile(const std::filesystem::path& path)
{
    const std::string extension = lower(path.extension().string());
    return extension == ".gltf" || extension == ".glb";
}

bool ignoredModel(const std::filesystem::path& path)
{
    const std::string stem = lower(path.stem().string());
    static constexpr std::array<std::string_view, 9> ignored{{
        "_physics",
        "_phys",
        "_mag",
        "_magazine",
        "_clip",
        "_shell",
        "_scope",
        "_slide",
        "_lod",
    }};
    for (std::string_view token : ignored)
        if (stem.ends_with(token)) return true;
    return false;
}

std::vector<std::filesystem::path> files(
    const std::filesystem::path& root,
    bool primary_weapons = false)
{
    std::vector<std::filesystem::path> result;
    std::error_code error;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::skip_permission_denied,
             error),
         end;
         !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error)) continue;
        const std::filesystem::path path = it->path().lexically_normal();
        if (!modelFile(path)) continue;

        if (primary_weapons) {
            const std::string stem = lower(path.stem().string());
            if (!stem.starts_with("weapon_") || ignoredModel(path)) continue;
        }
        result.push_back(path);
    }

    std::sort(result.begin(), result.end());
    return result;
}

int tokenScore(const std::filesystem::path& path, const WeaponSpec& spec)
{
    const std::string full = pathText(path);
    if (spec.exclude && full.find(spec.exclude) != std::string::npos) return -1;

    const std::string stem = lower(path.stem().string());
    const std::string parent = lower(path.parent_path().filename().string());

    const auto score = [&](const char *token) {
        if (!token || !*token) return -1;
        const std::string value = lower(token);
        int result = -1;

        if (parent == value) result = std::max(result, 120);
        if (stem == value || stem.ends_with("_" + value)) result = std::max(result, 110);
        if (full.find("/" + value + "/") != std::string::npos) result = std::max(result, 100);
        if (stem.find(value) != std::string::npos) result = std::max(result, 70);
        if (full.find(value) != std::string::npos) result = std::max(result, 40);
        return result;
    };

    return std::max(score(spec.token), score(spec.alternate));
}

std::filesystem::path bestModel(
    const std::vector<std::filesystem::path>& models,
    const WeaponSpec& spec)
{
    int best_score = -1;
    std::filesystem::path best;

    for (const auto& path : models) {
        const int score = tokenScore(path, spec);
        if (score < 0) continue;
        if (score > best_score ||
            (score == best_score && path.generic_string().size() < best.generic_string().size())) {
            best_score = score;
            best = path;
        }
    }
    return best;
}

bool actionMatch(const std::filesystem::path& path, Action action)
{
    const std::string stem = lower(path.stem().string());
    const auto has = [&](std::string_view value) {
        return stem.find(value) != std::string::npos;
    };

    switch (action) {
        case Action::Idle:
            return has("idle");
        case Action::Shoot:
            return has("shoot") || has("fire") || has("attack");
        case Action::Reload:
            return has("reload");
        case Action::Inspect:
            return has("lookat") || has("inspect");
    }
    return false;
}

bool familyMatches(const std::filesystem::path& family, const WeaponSpec& spec)
{
    const std::string name = lower(family.filename().string());
    const std::string full = pathText(family);
    if (spec.exclude && full.find(spec.exclude) != std::string::npos) return false;

    const auto matches = [&](const char *token) {
        if (!token || !*token) return false;
        const std::string value = lower(token);
        return name == value ||
               name.ends_with("_" + value) ||
               full.find("/" + value + "/") != std::string::npos ||
               name.find(value) != std::string::npos;
    };

    return matches(spec.token) || matches(spec.alternate);
}

std::filesystem::path matchingFamily(
    const std::filesystem::path& animation,
    const WeaponSpec& spec)
{
    for (std::filesystem::path family = animation.parent_path();
         !family.empty();
         family = family.parent_path()) {
        if (familyMatches(family, spec))
            return family;
    }
    return {};
}

bool insideFamily(
    const std::filesystem::path& path,
    const std::filesystem::path& family)
{
    if (family.empty()) return false;

    const std::filesystem::path relative = path.lexically_relative(family);
    if (relative.empty() || relative.is_absolute()) return false;

    for (const auto& part : relative)
        if (part == "..") return false;

    return true;
}

bool familyHasAction(
    const std::vector<std::filesystem::path>& animations,
    const std::filesystem::path& family,
    Action action)
{
    return std::any_of(
        animations.begin(),
        animations.end(),
        [&](const std::filesystem::path& path) {
            return insideFamily(path, family) && actionMatch(path, action);
        }
    );
}

std::filesystem::path animationFamily(
    const std::vector<std::filesystem::path>& animations,
    const WeaponSpec& spec)
{
    int best_score = -1;
    std::filesystem::path best;

    for (const auto& path : animations) {
        const std::filesystem::path family = matchingFamily(path, spec);
        if (family.empty() ||
            !familyHasAction(animations, family, Action::Idle) ||
            !familyHasAction(animations, family, Action::Shoot) ||
            !familyHasAction(animations, family, Action::Reload))
            continue;

        const int score = tokenScore(family, spec);
        if (score > best_score ||
            (score == best_score && family.generic_string().size() < best.generic_string().size())) {
            best_score = score;
            best = family;
        }
    }

    return best;
}

std::filesystem::path bestAnimation(
    const std::vector<std::filesystem::path>& animations,
    const std::filesystem::path& family,
    Action action)
{
    if (family.empty()) return {};

    int best_score = -1;
    std::filesystem::path best;

    for (const auto& path : animations) {
        if (!insideFamily(path, family) || !actionMatch(path, action)) continue;

        const std::string stem = lower(path.stem().string());
        int score = 0;

        switch (action) {
            case Action::Idle:
                if (stem.starts_with("idle")) score += 30;
                if (stem.find("idle") != std::string::npos) score += 10;
                break;
            case Action::Shoot:
                if (stem.starts_with("shoot")) score += 30;
                else if (stem.starts_with("fire")) score += 25;
                else if (stem.starts_with("attack")) score += 20;
                if (stem.find("shoot1") != std::string::npos) score += 5;
                break;
            case Action::Reload:
                if (stem.starts_with("reload")) score += 30;
                if (stem.find("reload") != std::string::npos) score += 10;
                break;
            case Action::Inspect:
                if (stem.starts_with("lookat01")) score += 30;
                else if (stem.starts_with("lookat")) score += 25;
                else if (stem.starts_with("inspect")) score += 20;
                break;
        }

        if (score > best_score ||
            (score == best_score && path.generic_string().size() < best.generic_string().size())) {
            best_score = score;
            best = path;
        }
    }

    return best;
}

std::vector<WeaponItem> discoverWeapons(const std::filesystem::path& root)
{
    const std::vector<std::filesystem::path> models =
        files(root / "Models/weapons/models", true);
    const std::vector<std::filesystem::path> animations =
        files(root / "Anim/animation/anims/viewmodel");

    std::vector<WeaponItem> result;
    result.reserve(WeaponSpecs.size());

    for (const WeaponSpec& spec : WeaponSpecs) {
        WeaponItem item;
        item.name = spec.name;
        item.category = spec.category;
        item.path = bestModel(models, spec);

        const std::filesystem::path family = animationFamily(animations, spec);
        item.idle = bestAnimation(animations, family, Action::Idle);
        item.shoot = bestAnimation(animations, family, Action::Shoot);
        item.reload = bestAnimation(animations, family, Action::Reload);
        item.inspect = bestAnimation(animations, family, Action::Inspect);

        if (item.name == "R8 Revolver") {
            const std::filesystem::path model = root / DefaultWeapon;
            const std::filesystem::path idle = root / DefaultIdle;
            const std::filesystem::path shoot = root / DefaultShoot;
            const std::filesystem::path reload = root / DefaultReload;
            const std::filesystem::path inspect = root / DefaultInspect;
            std::error_code error;
            if (std::filesystem::is_regular_file(model, error)) item.path = model.lexically_normal();
            error.clear();
            if (std::filesystem::is_regular_file(idle, error)) item.idle = idle.lexically_normal();
            error.clear();
            if (std::filesystem::is_regular_file(shoot, error)) item.shoot = shoot.lexically_normal();
            error.clear();
            if (std::filesystem::is_regular_file(reload, error)) item.reload = reload.lexically_normal();
            error.clear();
            if (std::filesystem::is_regular_file(inspect, error)) item.inspect = inspect.lexically_normal();
        }

        if (item.path.empty() ||
            item.idle.empty() ||
            item.shoot.empty() ||
            item.reload.empty())
            continue;

        if (item.inspect.empty())
            item.inspect = item.idle;

        result.push_back(std::move(item));
    }

    return result;
}

std::string label(const std::filesystem::path& path)
{
    std::string value = path.stem().string();
    constexpr std::array<std::string_view, 2> prefixes{{"glove_", "arms_"}};
    for (std::string_view prefix : prefixes) {
        if (value.starts_with(prefix)) {
            value.erase(0u, prefix.size());
            break;
        }
    }
    std::replace(value.begin(), value.end(), '_', ' ');
    return value;
}

std::vector<Item> discoverArms(const std::filesystem::path& root)
{
    std::vector<Item> result;
    for (const auto& path : files(root)) {
        const std::string stem = lower(path.stem().string());
        if (!stem.starts_with("glove_") && stem.find("arms") == std::string::npos) continue;
        if (ignoredModel(path)) continue;
        result.push_back(Item{label(path), path});
    }

    std::sort(result.begin(), result.end(), [](const Item& a, const Item& b) {
        if (a.name != b.name) return a.name < b.name;
        return a.path < b.path;
    });
    result.erase(
        std::unique(result.begin(), result.end(), [](const Item& a, const Item& b) {
            return a.path == b.path;
        }),
        result.end()
    );
    return result;
}

std::size_t findWeapon(const State& state, std::string_view name)
{
    for (std::size_t index = 0u; index < state.weapons.size(); ++index)
        if (state.weapons[index].name == name) return index;
    return std::numeric_limits<std::size_t>::max();
}

std::size_t ensureArm(
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
    if (model == Models::INVALID_MODEL)
        fail(state, message.empty() ? "Could not load " + path.string() : message, error);
    return model;
}

bool loadAnimations(
    State& state,
    const WeaponItem& weapon,
    AnimationHandles *handles,
    std::string *error)
{
    if (!handles) return fail(state, "Animation output is null", error);
    *handles = {};

    const auto missing = [&](const char *action) {
        return fail(
            state,
            "Could not resolve " + std::string(action) + " animation for " + weapon.name,
            error
        );
    };

    if (weapon.idle.empty()) return missing("idle");
    if (weapon.shoot.empty()) return missing("shoot");
    if (weapon.reload.empty()) return missing("reload");

    const std::filesystem::path inspect =
        weapon.inspect.empty() ? weapon.idle : weapon.inspect;

    handles->idle = load(state, weapon.idle, error);
    if (handles->idle == Models::INVALID_MODEL) return false;
    handles->shoot = load(state, weapon.shoot, error);
    if (handles->shoot == Models::INVALID_MODEL) return false;
    handles->reload = load(state, weapon.reload, error);
    if (handles->reload == Models::INVALID_MODEL) return false;
    handles->inspect = load(state, inspect, error);
    if (handles->inspect == Models::INVALID_MODEL) return false;
    return true;
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
    Models::ModelHandle idle,
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
    if (!Renderer::ModelScene::play(animation, idle, 0u, true, &message))
        return fail(
            state,
            message.empty() ? "Could not play viewmodel idle animation" : message,
            error
        );
    return true;
}

bool play(State& state, Models::ModelHandle animation, std::string *error)
{
    std::string message;
    if (!Renderer::ModelScene::play(state.animation, animation, 0u, false, &message))
        return fail(
            state,
            message.empty() ? "Could not play viewmodel animation" : message,
            error
        );
    state.error.clear();
    if (error) error->clear();
    return true;
}

} // namespace

const char *categoryName(WeaponCategory category)
{
    switch (category) {
        case WeaponCategory::Pistols: return "Pistols";
        case WeaponCategory::Smgs: return "SMGs";
        case WeaponCategory::Rifles: return "Rifles";
        case WeaponCategory::Snipers: return "Snipers";
        case WeaponCategory::Shotguns: return "Shotguns";
        case WeaponCategory::MachineGuns: return "Machine Guns";
    }
    return "Weapons";
}

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

    state.weapons = discoverWeapons(state.root);
    state.arms = discoverArms(state.root / "Arms/agents/models/shared/arms");

    state.weapon = findWeapon(state, "R8 Revolver");
    if (state.weapon == std::numeric_limits<std::size_t>::max())
        return fail(state, "Default R8 Revolver model or animations are incomplete", error);

    const std::filesystem::path arm_path = (state.root / DefaultArms).lexically_normal();
    state.arm = ensureArm(state.arms, arm_path);

    const Models::ModelHandle arms = load(state, arm_path, error);
    if (arms == Models::INVALID_MODEL) return false;

    const WeaponItem& selected = state.weapons[state.weapon];
    const Models::ModelHandle weapon = load(state, selected.path, error);
    if (weapon == Models::INVALID_MODEL) return false;

    AnimationHandles handles;
    if (!loadAnimations(state, selected, &handles, error)) return false;

    state.arm_instance = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            arms,
            state.arm_instance.get(),
            options(state),
            error))
        return fail(
            state,
            error && !error->empty() ? *error : "Could not instantiate viewmodel arms",
            error
        );

    state.weapon_instance = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            weapon,
            state.weapon_instance.get(),
            options(state),
            error))
        return fail(
            state,
            error && !error->empty() ? *error : "Could not instantiate viewmodel weapon",
            error
        );

    Renderer::ModelScene::Animation animation;
    if (!bind(
            state,
            animation,
            *state.arm_instance,
            *state.weapon_instance,
            handles.idle,
            error))
        return false;

    state.idle = handles.idle;
    state.shoot = handles.shoot;
    state.reload = handles.reload;
    state.inspect = handles.inspect;
    state.animation = std::move(animation);
    state.error.clear();
    if (error) error->clear();
    return true;
}

bool selectWeapon(State& state, Ecs::World& world, std::size_t index)
{
    if (state.shooting) return true;
    if (index >= state.weapons.size())
        return fail(state, "Invalid weapon selection");
    if (index == state.weapon) return true;

    const WeaponItem& selected = state.weapons[index];
    std::string message;

    const Models::ModelHandle model = load(state, selected.path, &message);
    if (model == Models::INVALID_MODEL) return false;

    AnimationHandles handles;
    if (!loadAnimations(state, selected, &handles, &message)) return false;

    auto replacement = std::make_unique<Renderer::ModelScene::Instance>();
    if (!Renderer::ModelScene::instantiate(
            world,
            model,
            replacement.get(),
            options(state),
            &message))
        return fail(
            state,
            message.empty() ? "Could not instantiate selected weapon" : message
        );

    Renderer::ModelScene::Animation animation;
    if (!bind(
            state,
            animation,
            *state.arm_instance,
            *replacement,
            handles.idle,
            &message)) {
        Renderer::ModelScene::destroy(world, *replacement);
        return false;
    }

    Renderer::ModelScene::destroy(world, *state.weapon_instance);
    state.weapon_instance = std::move(replacement);
    state.weapon = index;
    state.idle = handles.idle;
    state.shoot = handles.shoot;
    state.reload = handles.reload;
    state.inspect = handles.inspect;
    state.animation = std::move(animation);
    state.error.clear();
    return true;
}

bool selectArms(State& state, Ecs::World& world, std::size_t index)
{
    if (state.shooting) return true;
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
        return fail(
            state,
            message.empty() ? "Could not instantiate selected arms" : message
        );

    Renderer::ModelScene::Animation animation;
    if (!bind(
            state,
            animation,
            *replacement,
            *state.weapon_instance,
            state.idle,
            &message)) {
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
    if (state.shooting) return true;
    if (!play(state, state.shoot, error)) return false;
    state.shooting = true;
    return true;
}

bool reload(State& state, std::string *error)
{
    if (state.shooting) return true;
    return play(state, state.reload, error);
}

bool inspect(State& state, std::string *error)
{
    if (state.shooting) return true;
    return play(state, state.inspect, error);
}

bool update(State& state, Ecs::World& world, float delta_seconds, std::string *error)
{
    std::string message;
    if (!Renderer::ModelScene::update(world, state.animation, delta_seconds, &message))
        return fail(
            state,
            message.empty() ? "Could not update viewmodel animation" : message,
            error
        );

    if (state.shooting && !Renderer::ModelScene::playing(state.animation))
        state.shooting = false;

    if (!Renderer::ModelScene::playing(state.animation) &&
        !Renderer::ModelScene::play(state.animation, state.idle, 0u, true, &message))
        return fail(
            state,
            message.empty() ? "Could not restart viewmodel idle animation" : message,
            error
        );

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
    state.shooting = false;
    state.error.clear();
}

} // namespace Loadout
