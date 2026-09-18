#include "Loadout/Fire.hpp"

#include <cassert>
#include <cmath>

namespace {

bool near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

void semiAutoWaitsForShotAnimation()
{
    const Loadout::Fire::Profile profile{
        .mode = Loadout::Fire::Mode::Semi,
        .cycle_seconds = 0.15f,
    };
    Loadout::Fire::State state;
    Loadout::Fire::reset(state, profile);

    auto result = Loadout::Fire::step(state, profile, true, true, false, 0.0f);
    assert(result.shots == 1u);

    result = Loadout::Fire::step(state, profile, true, true, true, 0.20f);
    assert(result.shots == 0u);

    result = Loadout::Fire::step(state, profile, true, true, false, 0.0f);
    assert(result.shots == 1u);
}

void automaticFireRestartsAtCycleTime()
{
    const Loadout::Fire::Profile profile{
        .mode = Loadout::Fire::Mode::Automatic,
        .cycle_seconds = 0.075f,
    };
    Loadout::Fire::State state;
    Loadout::Fire::reset(state, profile);

    auto result = Loadout::Fire::step(state, profile, true, true, false, 0.0f);
    assert(result.shots == 1u);

    result = Loadout::Fire::step(state, profile, false, true, true, 0.05f);
    assert(result.shots == 0u);

    result = Loadout::Fire::step(state, profile, false, true, true, 0.025f);
    assert(result.shots == 1u);
}

void burstFiresThreeTimedShots()
{
    const Loadout::Fire::Profile profile{
        .mode = Loadout::Fire::Mode::Semi,
        .cycle_seconds = 0.15f,
        .burst_supported = true,
        .burst_count = 3u,
        .burst_cycle_seconds = 0.50f,
        .burst_interval_seconds = 0.05f,
    };
    Loadout::Fire::State state;
    Loadout::Fire::reset(state, profile);

    assert(Loadout::Fire::toggle(state, profile));
    assert(state.mode == Loadout::Fire::Mode::Burst);

    auto result = Loadout::Fire::step(state, profile, true, true, false, 0.0f);
    assert(result.shots == 1u);

    result = Loadout::Fire::step(state, profile, false, false, true, 0.05f);
    assert(result.shots == 1u);
    result = Loadout::Fire::step(state, profile, false, false, true, 0.05f);
    assert(result.shots == 1u);

    result = Loadout::Fire::step(state, profile, true, true, true, 0.39f);
    assert(result.shots == 0u);
    result = Loadout::Fire::step(state, profile, true, true, true, 0.01f);
    assert(result.shots == 1u);
}

void alternateFireOnlyTogglesSupportedProfiles()
{
    const Loadout::Fire::Profile plain{
        .mode = Loadout::Fire::Mode::Automatic,
        .cycle_seconds = 0.10f,
    };
    Loadout::Fire::State plain_state;
    Loadout::Fire::reset(plain_state, plain);
    assert(!Loadout::Fire::toggle(plain_state, plain));
    assert(plain_state.mode == Loadout::Fire::Mode::Automatic);

    const Loadout::Fire::Profile famas{
        .mode = Loadout::Fire::Mode::Automatic,
        .cycle_seconds = 0.09f,
        .burst_supported = true,
        .burst_count = 3u,
        .burst_cycle_seconds = 0.55f,
        .burst_interval_seconds = 0.075f,
    };
    Loadout::Fire::State famas_state;
    Loadout::Fire::reset(famas_state, famas);
    assert(Loadout::Fire::toggle(famas_state, famas));
    assert(famas_state.mode == Loadout::Fire::Mode::Burst);
    assert(Loadout::Fire::toggle(famas_state, famas));
    assert(famas_state.mode == Loadout::Fire::Mode::Automatic);
    assert(near(famas_state.trigger_cooldown, 0.0f));
}

} // namespace

int main()
{
    semiAutoWaitsForShotAnimation();
    automaticFireRestartsAtCycleTime();
    burstFiresThreeTimedShots();
    alternateFireOnlyTogglesSupportedProfiles();
    return 0;
}
