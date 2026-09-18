#ifndef GAME_LOADOUT_FIRING_HPP
#define GAME_LOADOUT_FIRING_HPP

#include "Fire.hpp"
#include "Loadout.hpp"

#include <cstddef>
#include <limits>
#include <string>

namespace Loadout::Firing {

struct Controller
{
    std::size_t weapon = std::numeric_limits<std::size_t>::max();
    Fire::State fire;
};

bool update(
    Controller& controller,
    State& loadout,
    bool primary_pressed,
    bool primary_held,
    bool alternate_pressed,
    float delta_seconds,
    std::string *error = nullptr
);

Fire::Mode mode(const Controller& controller);

} // namespace Loadout::Firing

#endif
