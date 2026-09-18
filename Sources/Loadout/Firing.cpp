#include "Firing.hpp"

#include <Renderer/ModelScene.hpp>

namespace Loadout::Firing {
namespace {

bool fail(State& loadout, const std::string& message, std::string *error)
{
    loadout.error = message;
    if (error) *error = message;
    return false;
}

const WeaponItem *selectedWeapon(const State& loadout)
{
    if (loadout.weapon >= loadout.weapons.size()) return nullptr;
    return &loadout.weapons[loadout.weapon];
}

} // namespace

bool update(
    Controller& controller,
    State& loadout,
    bool primary_pressed,
    bool primary_held,
    bool alternate_pressed,
    float delta_seconds,
    std::string *error)
{
    const WeaponItem *weapon = selectedWeapon(loadout);
    if (!weapon) return fail(loadout, "Could not resolve selected weapon fire profile", error);

    const Fire::Profile profile = Fire::profile(weapon->name);
    if (controller.weapon != loadout.weapon) {
        controller.weapon = loadout.weapon;
        Fire::reset(controller.fire, profile);
    }

    if (alternate_pressed && !loadout.shooting)
        (void)Fire::toggle(controller.fire, profile);

    const Fire::Result result = Fire::step(
        controller.fire,
        profile,
        primary_pressed,
        primary_held,
        loadout.shooting,
        delta_seconds
    );

    if (result.shots == 0u) {
        if (error) error->clear();
        return true;
    }

    if (loadout.shoot == Models::INVALID_MODEL)
        return fail(loadout, "Selected weapon has no shoot animation", error);

    std::string message;
    for (std::uint8_t shot = 0u; shot < result.shots; ++shot) {
        if (!Renderer::ModelScene::play(
                loadout.animation,
                loadout.shoot,
                0u,
                false,
                &message))
            return fail(
                loadout,
                message.empty() ? "Could not play weapon shoot animation" : message,
                error
            );
    }

    loadout.shooting = true;
    loadout.error.clear();
    if (error) error->clear();
    return true;
}

Fire::Mode mode(const Controller& controller)
{
    return controller.fire.mode;
}

} // namespace Loadout::Firing
