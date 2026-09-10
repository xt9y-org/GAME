#ifndef GAME_SCENES_SCENE_HPP
#define GAME_SCENES_SCENE_HPP

#include "Sources/Ecs/Ecs.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace Game::Scenes {

class Scene {
public:
    virtual ~Scene() = default;

    virtual const char *name() const = 0;
    virtual bool load(Ecs::World& world, std::string& error) = 0;
    virtual void update(Ecs::World& world, float delta_seconds)
    {
        (void)world;
        (void)delta_seconds;
    }

    virtual Ecs::Entity camera() const = 0;
    virtual std::size_t triangleCount() const = 0;

    virtual bool usesFreeCamera() const { return true; }
    virtual bool showStats() const { return true; }
    virtual void drawDebug(Ecs::World& world) { (void)world; }
    virtual void emitMetrics(const std::function<void(std::string_view, double)>& emit) const
    {
        (void)emit;
    }
};

} // namespace Game::Scenes

#endif
