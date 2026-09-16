#ifndef GAME_TESTS_FIXTURES_SCENE_FIXTURE_HPP
#define GAME_TESTS_FIXTURES_SCENE_FIXTURE_HPP

#include "Camera/Camera.hpp"
#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/Environment.hpp"
#include "Renderer/ModelScene.hpp"

#include <string>

namespace Testing {

struct SceneAssets {
    Models::ModelHandle model = Models::INVALID_MODEL;
};

inline SceneAssets triangleAssets(std::string *error = nullptr)
{
    return {Models::load("Assets/Models/visual-triangle.obj", error)};
}

inline Ecs::Entity addCamera(Ecs::World& world)
{
    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::Transform>(entity, Renderer::Transform{});
    world.add<Camera::CameraComponent>(entity, Camera::CameraComponent{
        .fov_degrees = 60.0f,
        .near_plane = 0.05f,
        .active = true,
        .projection = Camera::Projection::Perspective,
        .far_plane = 100.0f,
    });
    return entity;
}

inline Ecs::Entity addTriangle(
    Ecs::World& world,
    const SceneAssets& assets,
    Renderer::Vec3 position = {0.0f, 0.0f, -3.0f},
    std::string *error = nullptr)
{
    if (assets.model == Models::INVALID_MODEL) {
        if (error) *error = "triangle fixture model is invalid";
        return Ecs::INVALID_ENTITY;
    }

    const Ecs::Entity root = world.createEntity();
    world.add<Renderer::Transform>(root, Renderer::Transform{.position = position});

    Renderer::ModelScene::Instance instance;
    const Renderer::ModelScene::Options options{.parent = root};
    if (!Renderer::ModelScene::instantiate(world, assets.model, &instance, options, error)) {
        world.destroyEntity(root);
        return Ecs::INVALID_ENTITY;
    }
    if (instance.loose_parts.empty()) {
        if (error) *error = "triangle fixture produced no renderable part";
        return Ecs::INVALID_ENTITY;
    }
    return instance.loose_parts.front().entity;
}

inline void addLighting(Ecs::World& world)
{
    const Ecs::Entity light = world.createEntity();
    world.add<Renderer::Transform>(light, Renderer::Transform{.rotation = {-25.0f, 35.0f, 0.0f}});
    world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
        .type = Renderer::LightType::Directional,
        .color = {1.0f, 0.95f, 0.9f},
        .intensity = 2.0f,
    });
    world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{});

    const Ecs::Entity environment = world.createEntity();
    world.add<Renderer::EnvironmentComponent>(environment, Renderer::EnvironmentComponent{
        .sky_color = {0.03f, 0.04f, 0.06f},
        .ambient_intensity = 0.1f,
    });
}

} // namespace Testing

#endif
