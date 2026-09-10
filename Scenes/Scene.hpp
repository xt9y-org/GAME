#ifndef GAME_SCENES_SCENE_HPP
#define GAME_SCENES_SCENE_HPP

#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Renderer/Render.hpp"

#include <cstddef>
#include <string>

namespace Game::Scenes {

struct Renderers {
    Renderer::Rasterizer& rasterizer;
    Renderer::RayTracer& ray_tracer;
    Renderer::PathTracer& path_tracer;
};

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
    virtual void configure(Renderers& renderers)
    {
        (void)renderers;
    }

    virtual Ecs::Entity camera() const = 0;
    virtual std::size_t triangleCount() const = 0;
};

} // namespace Game::Scenes

#endif
