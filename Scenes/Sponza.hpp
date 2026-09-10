#ifndef GAME_SCENES_SPONZA_HPP
#define GAME_SCENES_SPONZA_HPP

#include "Scenes/Scene.hpp"

#include "Sources/Camera.hpp"
#include "Sources/Models/Models.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace Game::Scenes {

class Sponza final : public Scene {
public:
    const char *name() const override
    {
        return "Sponza";
    }

    void configure(Renderers& renderers) override
    {
        renderers.ray_tracer.settings().resolution_divisor = 4;
        renderers.ray_tracer.settings().exposure = 1.05f;
        renderers.path_tracer.settings().resolution_divisor = 2;
        renderers.path_tracer.settings().samples_per_frame = 2;
        renderers.path_tracer.settings().exposure = 1.05f;
    }

    bool load(Ecs::World& world, std::string& error) override
    {
        camera_ = Ecs::INVALID_ENTITY;
        triangle_count_ = 0u;

        camera_ = world.createEntity();
        world.add<Renderer::Transform>(camera_, Renderer::Transform{
            .position = {0.0f, 1.5f, 5.0f},
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
            60.0f, 0.1f, true
        });

        const Models::ModelHandle model = Models::load("Assets/Sponza/sponza.obj", &error);
        if (model == Models::INVALID_MODEL) return false;
        if (Models::partCount(model) == 0u) {
            error = "Sponza model has no renderable parts";
            return false;
        }

        float min_x = std::numeric_limits<float>::infinity();
        float min_y = std::numeric_limits<float>::infinity();
        float min_z = std::numeric_limits<float>::infinity();
        float max_x = -std::numeric_limits<float>::infinity();
        float max_y = -std::numeric_limits<float>::infinity();
        float max_z = -std::numeric_limits<float>::infinity();

        for (std::size_t index = 0u; index < Models::partCount(model); ++index) {
            const Models::ModelPart *part = Models::part(model, index);
            if (!part) continue;
            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh) continue;

            triangle_count_ += mesh->indices.size() / 3u;
            min_x = std::min(min_x, mesh->bounds.minimum.x);
            min_y = std::min(min_y, mesh->bounds.minimum.y);
            min_z = std::min(min_z, mesh->bounds.minimum.z);
            max_x = std::max(max_x, mesh->bounds.maximum.x);
            max_y = std::max(max_y, mesh->bounds.maximum.y);
            max_z = std::max(max_z, mesh->bounds.maximum.z);

            const Ecs::Entity entity = world.createEntity();
            world.add<Renderer::Transform>(entity, Renderer::Transform{});
            world.add<Renderer::MeshComponent>(
                entity,
                Renderer::MeshComponent{part->mesh, part->material}
            );
            world.add<Renderer::RenderableComponent>(
                entity,
                Renderer::RenderableComponent{true}
            );
        }

        const bool valid_bounds =
            std::isfinite(min_x) && std::isfinite(min_y) && std::isfinite(min_z) &&
            std::isfinite(max_x) && std::isfinite(max_y) && std::isfinite(max_z);
        if (!valid_bounds) {
            error = "Sponza model has invalid bounds";
            return false;
        }

        const float extent_x = std::max(max_x - min_x, 1.0f);
        const float extent_y = std::max(max_y - min_y, 1.0f);
        const float extent_z = std::max(max_z - min_z, 1.0f);
        const float scene_radius = std::max({extent_x, extent_y, extent_z});

        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .position = {
                (min_x + max_x) * 0.5f,
                min_y + extent_y * 0.78f,
                (min_z + max_z) * 0.5f,
            },
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {1.0f, 0.96f, 0.90f},
            .intensity = scene_radius * scene_radius * 3.0f,
        });

        const Ecs::Entity gi = world.createEntity();
        world.add<Renderer::GlobalIlluminationComponent>(
            gi,
            Renderer::GlobalIlluminationComponent{
                .enabled = true,
                .intensity = 1.0f,
                .bounces = 2,
            }
        );

        world.markChanged();
        return true;
    }

    Ecs::Entity camera() const override
    {
        return camera_;
    }

    std::size_t triangleCount() const override
    {
        return triangle_count_;
    }

private:
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    std::size_t triangle_count_ = 0u;
};

} // namespace Game::Scenes

#endif
