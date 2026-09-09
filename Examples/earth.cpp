#include "earth.hpp"

#include "Font.hpp"
#include "Renderer/Components.hpp"
#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Core/Texture.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"

#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class Example
{
private:
    Renderer::PathTracer *renderer_ = new Renderer::PathTracer();
    Camera::Controller *camera_controller_ = new Camera::Controller();
    Animation::System *animation_system_ = new Animation::System();
    Ecs::World *world_ = new Ecs::World();
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;

public:
    Example(const char* _title, const std::vector<int> _dim)
    {
        lwcglInstallFastRuntime();

#ifdef __APPLE__
        lwcglSetContextVersion(2, 1);
        lwcglSetContextProfile(LWCGL_CONTEXT_ANY_PROFILE);
#else
        lwcglSetContextVersion(4, 3);
        lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

        Display.setDisplayMode(new DisplayMode(_dim[0], _dim[1]));
        Display.create();
        Display.setTitle(_title);

        Keyboard.create();
        Mouse.create();

        renderer_->init();

        Renderer::PathTracerSettings& settings = renderer_->settings();
        settings.resolution_divisor = 2;
        settings.samples_per_frame = 2;
        settings.max_bounces = 1;
        settings.exposure = 1.05f;
    }

    ~Example()
    {
        renderer_->shutdown();
        delete renderer_;
        renderer_ = nullptr;

        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();

        delete animation_system_;
        delete camera_controller_;
        delete world_;
    }

    static inline int run(int argc, char **argv)
    {
        (void)argc;
        (void)argv;

        Example *e = new Example("Earth", {1280, 720});

        int _framebuffer_width  = std::max(Display.getWidth(),  1),
            _framebuffer_height = std::max(Display.getHeight(), 1);

        e->renderer_->resize(_framebuffer_width, _framebuffer_height);

        e->camera_ = e->world_->createEntity();

        e->world_->add<Renderer::Transform>(e->camera_, Renderer::Transform{});
        e->world_->add<Camera::CameraComponent>(e->camera_, Camera::CameraComponent{
            45.0f, 0.1f, true
        });

        std::string _error;
        const Models::ModelHandle _model = Models::load("Assets/Earth/Earth.fbx", &_error);

        if (_model == Models::INVALID_MODEL)
        {
            std::fprintf(stderr, "[LOG]: %s\n", _error.c_str());
            delete e;
            return 3;
        }

        if (Models::partCount(_model) == 0u)
        {
            std::fprintf(stderr, "[LOG]: model has no renderable parts\n");
            delete e;
            return 3;
        }

        std::size_t _surface_index = 0u;
        float _surface_extent = std::numeric_limits<float>::infinity();

        for (std::size_t i = 0; i < Models::partCount(_model); ++i)
        {
            const Models::ModelPart *part = Models::part(_model, i);
            if (!part) continue;
            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh) continue;

            const float extent_x = std::max(mesh->bounds.maximum.x - mesh->bounds.minimum.x, 0.0f);
            const float extent_y = std::max(mesh->bounds.maximum.y - mesh->bounds.minimum.y, 0.0f);
            const float extent_z = std::max(mesh->bounds.maximum.z - mesh->bounds.minimum.z, 0.0f);
            const float extent = std::max({extent_x, extent_y, extent_z});
            if (extent > 0.0f && extent < _surface_extent)
            {
                _surface_extent = extent;
                _surface_index = i;
            }
        }

        const Models::ModelPart *_surface_part = Models::part(_model, _surface_index);
        const Models::MeshData *_surface_mesh = _surface_part
            ? Models::mesh(_surface_part->mesh)
            : nullptr;

        if (!_surface_part || !_surface_mesh)
        {
            std::fprintf(stderr, "[LOG]: failed to select Earth surface shell\n");
            delete e;
            return 3;
        }

        const float _min_x = _surface_mesh->bounds.minimum.x;
        const float _min_y = _surface_mesh->bounds.minimum.y;
        const float _min_z = _surface_mesh->bounds.minimum.z;
        const float _max_x = _surface_mesh->bounds.maximum.x;
        const float _max_y = _surface_mesh->bounds.maximum.y;
        const float _max_z = _surface_mesh->bounds.maximum.z;

        const float _extent_x = std::max(_max_x - _min_x, 1.0e-4f);
        const float _extent_y = std::max(_max_y - _min_y, 1.0e-4f);
        const float _extent_z = std::max(_max_z - _min_z, 1.0e-4f);
        const float _globe_radius = std::max({_extent_x, _extent_y, _extent_z}) * 4.5f;
        const float _display_radius = std::max(_globe_radius * 1.5f, 1.5f);
        const float _center_x = (_min_x + _max_x) * 0.5f;
        const float _center_y = (_min_y + _max_y) * 0.5f;
        const float _center_z = (_min_z + _max_z) * 0.5f;

        std::string _texture_error;
        const Models::TextureHandle _earth_texture = Models::loadTexture(
            "Assets/Textures/earth_diffuse.png",
            &_texture_error
        );
        if (_earth_texture == Models::INVALID_TEXTURE)
        {
            std::fprintf(stderr, "[LOG]: Earth texture: %s\n", _texture_error.c_str());
            delete e;
            return 3;
        }

        Models::MaterialData _earth_surface;
        _earth_surface.name = "Earth diffuse surface";
        _earth_surface.color = {1.0f, 1.0f, 1.0f};
        _earth_surface.opacity = 1.0f;
        _earth_surface.texture_path = "Assets/Textures/earth_diffuse.png";
        _earth_surface.diffuse_texture = _earth_texture;

        const Models::MeshHandle _earth_mesh = Models::registerMesh(
            EarthDemo::makeSphere(_display_radius)
        );
        const Models::MaterialHandle _earth_material = Models::registerMaterial(
            std::move(_earth_surface)
        );

        if (_earth_mesh == Models::INVALID_MESH ||
            _earth_material == Models::INVALID_MATERIAL)
        {
            std::fprintf(stderr, "[LOG]: failed to register dense Earth mesh/material\n");
            delete e;
            return 3;
        }

        const Models::MeshData *_display_mesh = Models::mesh(_earth_mesh);
        const std::size_t _triangle_count = _display_mesh
            ? _display_mesh->indices.size() / 3u
            : 0u;

        if (Renderer::Transform *camera = e->world_->get<Renderer::Transform>(e->camera_))
        {
            camera->position = {
                .x = _center_x,
                .y = _center_y,
                .z = _center_z + std::max(_display_radius * 2.58f, 1.0f),
            };
            camera->rotation = {};
            camera->scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f};
            e->world_->markChanged();
        }

        const float _sun_orbit_radius = std::max(_display_radius * 3.0f, 1.0f);
        const float _sun_vertical_offset = _display_radius * 1.15f;

        const Ecs::Entity _light = e->world_->createEntity();

        e->world_->add<Renderer::Transform>(_light, Renderer::Transform{
            .position = {
                .x = _center_x + _sun_orbit_radius * 0.72f,
                .y = _center_y + _sun_vertical_offset,
                .z = _center_z + _sun_orbit_radius * 0.68f,
            },
            .rotation = {},
            .scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
        });

        e->world_->add<Renderer::LightComponent>(_light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {.x = 1.0f, .y = 0.96f, .z = 0.90f},
            .intensity = _sun_orbit_radius * _sun_orbit_radius * 1.8f,
        });

        const Ecs::Entity _earth = e->world_->createEntity();
        e->world_->add<Renderer::Transform>(_earth, Renderer::Transform{
            .position = {.x = _center_x, .y = _center_y, .z = _center_z},
            .rotation = {},
            .scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
        });
        e->world_->add<Renderer::MeshComponent>(
            _earth,
            Renderer::MeshComponent{_earth_mesh, _earth_material}
        );
        e->world_->add<Renderer::RenderableComponent>(
            _earth,
            Renderer::RenderableComponent{true}
        );

        const Ecs::Entity _stats = Font::screen(
            *e->world_,
            "",
            {12.0f, 12.0f},
            2.0f
        );

        Font::world(
            *e->world_, 
            "Felix Felix Felix Felix", 
            Renderer::Transform{
                .position = {0.0f, 2.0f, 40.0f},
                .rotation = {0.0f, 0.0f, 0.0f},
                .scale    = {1.0f, 1.0f, 1.0f},
            }
        );

        using Clock = std::chrono::steady_clock;
        auto _previous = Clock::now();

        while (!Display.isCloseRequested())
        {
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            const auto now = Clock::now();
            const float delta_seconds = std::chrono::duration<float>(now - _previous).count();
            _previous = now;
            const float frame_delta = std::min(delta_seconds, 0.1f);

            e->animation_system_->update(*e->world_, frame_delta);
            e->camera_controller_->update(*e->world_, frame_delta);

            if (Font::TextComponent *stats = e->world_->get<Font::TextComponent>(_stats))
            {
                const long fps = delta_seconds > 1.0e-6f
                    ? std::lround(1.0 / static_cast<double>(delta_seconds))
                    : 0L;
                stats->text =
                    "FPS: " + std::to_string(fps) +
                    "\nTriangles: " + std::to_string(_triangle_count);
            }

            const int width  = std::max(Display.getWidth(), 1);
            const int height = std::max(Display.getHeight(), 1);

            if ((width != _framebuffer_width) || height != _framebuffer_height)
            {
                _framebuffer_height = height;
                _framebuffer_width  = width;
                e->renderer_->resize(width, height);
            }

            e->renderer_->render(*e->world_);
#ifndef __APPLE__
            Display.updateNoMessages();
#endif
        }

        delete e;
        return 0;
    }
};

int main(int argc, char **argv)
{
    return Example::run(argc, argv);
}
