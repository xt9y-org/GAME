#ifndef GAME_DRIVING_GEOMETRY_HPP
#define GAME_DRIVING_GEOMETRY_HPP

#include "Sources/Models/Models.hpp"

namespace Game::Driving::Geometry {

Models::MeshHandle unitBox();
Models::MeshHandle road(float width, float length);
Models::MeshHandle laneMarkings(int lane_count, float lane_width, float length);
Models::MaterialHandle material(Models::Vec3 color);

} // namespace Game::Driving::Geometry

#endif
