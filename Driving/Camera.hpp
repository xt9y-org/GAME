#ifndef GAME_DRIVING_CAMERA_HPP
#define GAME_DRIVING_CAMERA_HPP

#include "Sources/Ecs/Ecs.hpp"

namespace Game::Driving {

void updateCamera(Ecs::World& world, float delta_seconds);

} // namespace Game::Driving

#endif
