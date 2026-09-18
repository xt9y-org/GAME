#ifndef GAME_LOADOUT_FIRE_HPP
#define GAME_LOADOUT_FIRE_HPP

#include <cstdint>

namespace Loadout::Fire {

enum class Mode : std::uint8_t
{
    Semi,
    Automatic,
    Burst,
};

struct Profile
{
    Mode mode = Mode::Semi;
    float cycle_seconds = 0.0f;
    bool burst_supported = false;
    std::uint8_t burst_count = 3u;
    float burst_cycle_seconds = 0.0f;
    float burst_interval_seconds = 0.0f;
};

struct State
{
    Mode mode = Mode::Semi;
    float trigger_cooldown = 0.0f;
    float burst_cooldown = 0.0f;
    std::uint8_t burst_remaining = 0u;
};

struct Result
{
    std::uint8_t shots = 0u;
};

void reset(State& state, const Profile& profile);
bool toggle(State& state, const Profile& profile);
Result step(
    State& state,
    const Profile& profile,
    bool pressed,
    bool held,
    bool shot_animation_active,
    float delta_seconds
);

} // namespace Loadout::Fire

#endif
