#ifndef GAME_TESTS_FIXTURES_SCENE_FIXTURE_HPP
#define GAME_TESTS_FIXTURES_SCENE_FIXTURE_HPP

#include "Camera.hpp"
#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/Environment.hpp"

#include <utility>

namespace Testing {

struct SceneAssets {
    Models::MeshHandle mesh = Models::INVALID_MESH;
    Models::MaterialHandle material = Models::INVALID_MATERIAL;
};

inline SceneAssets triangleAssets()
{
    Models::MeshData mesh;
    mesh.vertices = {
        Models::Vertex{.position = {-0.75f, -0.5f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}},
        Models::Vertex{.position = { 0.75f, -0.5f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}},
        Models::Vertex{.position = { 0.0f,   0.75f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}},
    };
    mesh.indices = {0u, 1u, 2u};
    mesh.bounds = {{-0.75f, -0.5f, -0.01f}, {0.75f, 0.75f, 0.01f}};

    Models::MaterialData material;
    material.name = "regression";
    material.color = {0.8f, 0.3f, 0.15f};
    material.roughness = 0.6f;
    material.metallic = 0.1f;

    return {Models::registerMesh(std::move(mesh)), Models::registerMaterial(std::move(material))};
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
    Renderer::Vec3 position = {0.0f, 0.0f, -3.0f})
{
    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::Transform>(entity, Renderer::Transform{.position = position});
    world.add<Renderer::MeshComponent>(entity, Renderer::MeshComponent{assets.mesh, assets.material});
    world.add<Renderer::RenderableComponent>(entity, Renderer::RenderableComponent{true});
    return entity;
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

    const Ecs::Entity environment = world.createEntity();
    world.add<Renderer::EnvironmentComponent>(environment, Renderer::EnvironmentComponent{
        .sky_color = {0.03f, 0.04f, 0.06f},
        .ambient_intensity = 0.1f,
    });
}

} // namespace Testing

#endif
