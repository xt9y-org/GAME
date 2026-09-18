#include "Fire.hpp"

#include <algorithm>

namespace Loadout::Fire {
namespace {

constexpr float Ready = 0.000001f;

float tick(float value, float delta_seconds)
{
    return std::max(value - std::max(delta_seconds, 0.0f), 0.0f);
}

Profile automatic(float cycle_seconds)
{
    return Profile{
        .mode = Mode::Automatic,
        .cycle_seconds = cycle_seconds,
    };
}

Profile burstable(
    Mode mode,
    float cycle_seconds,
    float burst_cycle_seconds,
    float burst_interval_seconds)
{
    return Profile{
        .mode = mode,
        .cycle_seconds = cycle_seconds,
        .burst_supported = true,
        .burst_count = 3u,
        .burst_cycle_seconds = burst_cycle_seconds,
        .burst_interval_seconds = burst_interval_seconds,
    };
}

} // namespace

Profile profile(std::string_view weapon_name)
{
    if (weapon_name == "Glock-18")
        return burstable(Mode::Semi, 0.15f, 0.50f, 0.05f);
    if (weapon_name == "CZ75-Auto") return automatic(0.10f);

    if (weapon_name == "MAC-10") return automatic(0.075f);
    if (weapon_name == "MP9") return automatic(0.070f);
    if (weapon_name == "MP7") return automatic(0.080f);
    if (weapon_name == "MP5-SD") return automatic(0.080f);
    if (weapon_name == "UMP-45") return automatic(0.090f);
    if (weapon_name == "P90") return automatic(0.070f);
    if (weapon_name == "PP-Bizon") return automatic(0.080f);

    if (weapon_name == "Galil AR") return automatic(0.090f);
    if (weapon_name == "FAMAS")
        return burstable(Mode::Automatic, 0.090f, 0.55f, 0.075f);
    if (weapon_name == "AK-47") return automatic(0.100f);
    if (weapon_name == "M4A4") return automatic(0.090f);
    if (weapon_name == "M4A1-S") return automatic(0.100f);
    if (weapon_name == "AUG") return automatic(0.100f);
    if (weapon_name == "SG 553") return automatic(0.110f);

    if (weapon_name == "XM1014") return automatic(0.350f);

    if (weapon_name == "M249") return automatic(0.080f);
    if (weapon_name == "Negev") return automatic(0.080f);

    return {};
}

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
