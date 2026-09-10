#ifndef GAME_SCENES_EARTH_HPP
#define GAME_SCENES_EARTH_HPP

#include "Scenes/Scene.hpp"

#include "Font.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Models/Core/Texture.hpp"
#include "Sources/Models/Models.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace Game::Scenes {

class Earth final : public Scene {
public:
    const char *name() const override
    {
        return "Earth";
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
        error.clear();
        camera_ = Ecs::INVALID_ENTITY;
        triangle_count_ = 0u;

        const Models::ModelHandle model = Models::load("Assets/Earth/Earth.fbx", &error);
        if (model == Models::INVALID_MODEL) return false;
        if (Models::partCount(model) == 0u) {
            error = "Earth model has no renderable parts";
            return false;
        }

        std::size_t surface_index = 0u;
        float surface_extent = std::numeric_limits<float>::infinity();
        for (std::size_t index = 0u; index < Models::partCount(model); ++index) {
            const Models::ModelPart *part = Models::part(model, index);
            if (!part) continue;
            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh) continue;

            const float extent_x = std::max(mesh->bounds.maximum.x - mesh->bounds.minimum.x, 0.0f);
            const float extent_y = std::max(mesh->bounds.maximum.y - mesh->bounds.minimum.y, 0.0f);
            const float extent_z = std::max(mesh->bounds.maximum.z - mesh->bounds.minimum.z, 0.0f);
            const float extent = std::max({extent_x, extent_y, extent_z});
            if (extent > 0.0f && extent < surface_extent) {
                surface_extent = extent;
                surface_index = index;
            }
        }

        const Models::ModelPart *surface_part = Models::part(model, surface_index);
        const Models::MeshData *surface_mesh = surface_part
            ? Models::mesh(surface_part->mesh)
            : nullptr;
        if (!surface_part || !surface_mesh || !std::isfinite(surface_extent)) {
            error = "Failed to select Earth surface shell";
            return false;
        }

        const float min_x = surface_mesh->bounds.minimum.x;
        const float min_y = surface_mesh->bounds.minimum.y;
        const float min_z = surface_mesh->bounds.minimum.z;
        const float max_x = surface_mesh->bounds.maximum.x;
        const float max_y = surface_mesh->bounds.maximum.y;
        const float max_z = surface_mesh->bounds.maximum.z;
        if (!std::isfinite(min_x) || !std::isfinite(min_y) || !std::isfinite(min_z) ||
            !std::isfinite(max_x) || !std::isfinite(max_y) || !std::isfinite(max_z))
        {
            error = "Earth model has invalid bounds";
            return false;
        }

        const float extent_x = std::max(max_x - min_x, 1.0e-4f);
        const float extent_y = std::max(max_y - min_y, 1.0e-4f);
        const float extent_z = std::max(max_z - min_z, 1.0e-4f);
        const float globe_radius = std::max({extent_x, extent_y, extent_z}) * 4.5f;
        const float display_radius = std::max(globe_radius * 1.5f, 1.5f);
        const Renderer::Vec3 center {
            (min_x + max_x) * 0.5f,
            (min_y + max_y) * 0.5f,
            (min_z + max_z) * 0.5f,
        };

        if (earth_mesh_ == Models::INVALID_MESH || earth_material_ == Models::INVALID_MATERIAL) {
            std::string texture_error;
            const Models::TextureHandle earth_texture = Models::loadTexture(
                "Assets/Textures/earth_diffuse.png",
                &texture_error
            );
            if (earth_texture == Models::INVALID_TEXTURE) {
                error = "Earth texture: " + texture_error;
                return false;
            }

            Models::MaterialData material;
            material.name = "Earth diffuse surface";
            material.color = {1.0f, 1.0f, 1.0f};
            material.opacity = 1.0f;
            material.texture_path = "Assets/Textures/earth_diffuse.png";
            material.diffuse_texture = earth_texture;

            earth_mesh_ = Models::registerMesh(makeSphere(display_radius));
            earth_material_ = Models::registerMaterial(std::move(material));
            if (earth_mesh_ == Models::INVALID_MESH || earth_material_ == Models::INVALID_MATERIAL) {
                error = "Failed to register Earth mesh/material";
                return false;
            }
        }

        const Models::MeshData *display_mesh = Models::mesh(earth_mesh_);
        if (!display_mesh) {
            error = "Earth display mesh is unavailable";
            return false;
        }
        triangle_count_ = display_mesh->indices.size() / 3u;

        camera_ = world.createEntity();
        world.add<Renderer::Transform>(camera_, Renderer::Transform{
            .position = {
                center.x,
                center.y,
                center.z + std::max(display_radius * 2.58f, 1.0f),
            },
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
            45.0f, 0.1f, true
        });

        const float sun_orbit_radius = std::max(display_radius * 3.0f, 1.0f);
        const float sun_vertical_offset = display_radius * 1.15f;
        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .position = {
                center.x + sun_orbit_radius * 0.72f,
                center.y + sun_vertical_offset,
                center.z + sun_orbit_radius * 0.68f,
            },
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {1.0f, 0.96f, 0.90f},
            .intensity = sun_orbit_radius * sun_orbit_radius * 1.8f,
        });

        const Ecs::Entity gi = world.createEntity();
        world.add<Renderer::GlobalIlluminationComponent>(
            gi,
            Renderer::GlobalIlluminationComponent{
                .enabled = true,
                .intensity = 1.0f,
                .bounces = 1,
            }
        );

        const Ecs::Entity earth = world.createEntity();
        world.add<Renderer::Transform>(earth, Renderer::Transform{
            .position = center,
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world.add<Renderer::MeshComponent>(
            earth,
            Renderer::MeshComponent{earth_mesh_, earth_material_}
        );
        world.add<Renderer::RenderableComponent>(
            earth,
            Renderer::RenderableComponent{true}
        );

        Font::world(
            world,
            "Felix Felix Felix Felix",
            Renderer::Transform{
                .position = {0.0f, 2.0f, 40.0f},
                .rotation = {},
                .scale = {1.0f, 1.0f, 1.0f},
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
    static Models::MeshData makeSphere(float radius)
    {
        constexpr std::size_t longitude_segments = 256u;
        constexpr std::size_t latitude_segments = 128u;
        constexpr float pi = 3.14159265358979323846f;

        Models::MeshData mesh;
        if (!(radius > 0.0f)) return mesh;

        const std::size_t stride = longitude_segments + 1u;
        mesh.vertices.reserve((latitude_segments + 1u) * stride);
        mesh.indices.reserve(longitude_segments * (latitude_segments - 1u) * 6u);

        for (std::size_t latitude = 0u; latitude <= latitude_segments; ++latitude) {
            const float v = static_cast<float>(latitude) / static_cast<float>(latitude_segments);
            const float theta = v * pi;
            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);

            for (std::size_t longitude = 0u; longitude <= longitude_segments; ++longitude) {
                const float u = static_cast<float>(longitude) / static_cast<float>(longitude_segments);
                const float phi = (u - 0.5f) * (2.0f * pi);
                const float nx = sin_theta * std::sin(phi);
                const float ny = cos_theta;
                const float nz = sin_theta * std::cos(phi);

                Models::Vertex vertex;
                vertex.position = {nx * radius, ny * radius, nz * radius};
                vertex.normal = {nx, ny, nz};
                vertex.uv = {u, 1.0f - v};
                mesh.vertices.push_back(vertex);
            }
        }

        for (std::size_t latitude = 0u; latitude < latitude_segments; ++latitude) {
            const std::size_t row0 = latitude * stride;
            const std::size_t row1 = (latitude + 1u) * stride;

            for (std::size_t longitude = 0u; longitude < longitude_segments; ++longitude) {
                const std::uint32_t a = static_cast<std::uint32_t>(row0 + longitude);
                const std::uint32_t b = static_cast<std::uint32_t>(row0 + longitude + 1u);
                const std::uint32_t c = static_cast<std::uint32_t>(row1 + longitude);
                const std::uint32_t d = static_cast<std::uint32_t>(row1 + longitude + 1u);

                if (latitude == 0u) {
                    mesh.indices.push_back(b);
                    mesh.indices.push_back(c);
                    mesh.indices.push_back(d);
                    continue;
                }

                if (latitude + 1u == latitude_segments) {
                    mesh.indices.push_back(a);
                    mesh.indices.push_back(c);
                    mesh.indices.push_back(b);
                    continue;
                }

                mesh.indices.push_back(a);
                mesh.indices.push_back(c);
                mesh.indices.push_back(b);
                mesh.indices.push_back(b);
                mesh.indices.push_back(c);
                mesh.indices.push_back(d);
            }
        }

        mesh.bounds.minimum = {-radius, -radius, -radius};
        mesh.bounds.maximum = { radius,  radius,  radius};
        return mesh;
    }

    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    std::size_t triangle_count_ = 0u;
    Models::MeshHandle earth_mesh_ = Models::INVALID_MESH;
    Models::MaterialHandle earth_material_ = Models::INVALID_MATERIAL;
};

} // namespace Game::Scenes

#endif