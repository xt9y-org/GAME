#include "Fire.hpp"

#include <algorithm>

namespace Loadout::Fire {
namespace {

constexpr float Ready = 0.000001f;

float tick(float value, float delta_seconds)
{
    return std::max(value - std::max(delta_seconds, 0.0f), 0.0f);
}

} // namespace

void reset(State& state, const Profile& profile)
{
    state.mode = profile.mode;
    state.trigger_cooldown = 0.0f;
    state.burst_cooldown = 0.0f;
    state.burst_remaining = 0u;
}

bool toggle(State& state, const Profile& profile)
{
    if (!profile.burst_supported) return false;

    state.mode = state.mode == Mode::Burst ? profile.mode : Mode::Burst;
    state.trigger_cooldown = 0.0f;
    state.burst_cooldown = 0.0f;
    state.burst_remaining = 0u;
    return true;
}

Result step(
    State& state,
    const Profile& profile,
    bool pressed,
    bool held,
    bool shot_animation_active,
    float delta_seconds)
{
    Result result;
    state.trigger_cooldown = tick(state.trigger_cooldown, delta_seconds);
    state.burst_cooldown = tick(state.burst_cooldown, delta_seconds);

    if (state.burst_remaining > 0u) {
        if (state.burst_cooldown <= Ready) {
            --state.burst_remaining;
            ++result.shots;
            if (state.burst_remaining > 0u)
                state.burst_cooldown = std::max(profile.burst_interval_seconds, 0.0f);
        }
        return result;
    }

    if (state.trigger_cooldown > Ready) return result;

    switch (state.mode) {
        case Mode::Semi:
            if (!pressed || shot_animation_active) return result;
            result.shots = 1u;
            state.trigger_cooldown = std::max(profile.cycle_seconds, 0.0f);
            return result;

        case Mode::Automatic:
            if (!held) return result;
            result.shots = 1u;
            state.trigger_cooldown = std::max(profile.cycle_seconds, 0.0f);
            return result;

        case Mode::Burst:
            if (!pressed) return result;
            result.shots = 1u;
            state.burst_remaining = profile.burst_count > 0u
                ? static_cast<std::uint8_t>(profile.burst_count - 1u)
                : 0u;
            state.burst_cooldown = std::max(profile.burst_interval_seconds, 0.0f);
            state.trigger_cooldown = std::max(profile.burst_cycle_seconds, 0.0f);
            return result;
    }

    return result;
}

} // namespace Loadout::Fire
