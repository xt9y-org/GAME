#ifndef GAME_DRIVING_DYNAMICS_HPP
#define GAME_DRIVING_DYNAMICS_HPP

#include "Sources/Ecs/Ecs.hpp"

namespace Game::Driving {

void updatePlayer(Ecs::World& world, float delta_seconds, bool input_enabled);

} // namespace Game::Driving

#endif
