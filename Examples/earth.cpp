#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
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
        Example *e = new Example("Earth", {1280, 720});

        int _framebuffer_width  = std::max(Display.getWidth(),  1),
            _framebuffer_height = std::max(Display.getHeight(), 1);

        e->renderer_->resize(_framebuffer_width, _framebuffer_height);

        e->camera_ = e->world_->createEntity();

        e->world_->add<Renderer::Transform>(e->camera_, Renderer::Transform{
            .position = {.x = 0.0f, .y = 1.5f, .z = 5.0f},
            .rotation = {.x = 0.0f, .y = 0.0f, .z = 0.0f},
            .scale    = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
        });

        e->world_->add<Camera::CameraComponent>(e->camera_, Camera::CameraComponent{
            60.0f, 0.1f, true
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

        float _min_x =  std::numeric_limits<float>::infinity();
        float _min_y =  std::numeric_limits<float>::infinity();
        float _min_z =  std::numeric_limits<float>::infinity();
        float _max_x = -std::numeric_limits<float>::infinity();
        float _max_y = -std::numeric_limits<float>::infinity();
        float _max_z = -std::numeric_limits<float>::infinity();

        for (std::size_t i = 0; i < Models::partCount(_model); ++i)
        {
            const Models::ModelPart *part = Models::part(_model, i);
            if (!part) continue;
            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh) continue;

            _min_x = std::min(_min_x, mesh->bounds.minimum.x);
            _min_y = std::min(_min_y, mesh->bounds.minimum.y);
            _min_z = std::min(_min_z, mesh->bounds.minimum.z);
            _max_x = std::max(_max_x, mesh->bounds.maximum.x);
            _max_y = std::max(_max_y, mesh->bounds.maximum.y);
            _max_z = std::max(_max_z, mesh->bounds.maximum.z);
        }

        const bool _valid_bounds =
            std::isfinite(_min_x) && std::isfinite(_min_y) && std::isfinite(_min_z) &&
            std::isfinite(_max_x) && std::isfinite(_max_y) && std::isfinite(_max_z);

        const float _extent_x = _valid_bounds ? std::max(_max_x - _min_x, 1.0f) : 20.0f;
        const float _extent_y = _valid_bounds ? std::max(_max_y - _min_y, 1.0f) : 20.0f;
        const float _extent_z = _valid_bounds ? std::max(_max_z - _min_z, 1.0f) : 20.0f;
        const float _globe_radius = std::max({_extent_x, _extent_y, _extent_z}) * 0.5f;
        const float _center_x = _valid_bounds ? (_min_x + _max_x) * 0.5f : 0.0f;
        const float _center_y = _valid_bounds ? (_min_y + _max_y) * 0.5f : 0.0f;
        const float _center_z = _valid_bounds ? (_min_z + _max_z) * 0.5f : 0.0f;
        const float _sun_orbit_radius = std::max(_globe_radius * 3.0f, 1.0f);
        const float _sun_vertical_offset = _globe_radius * 0.35f;
        constexpr float _sun_orbit_seconds = 25.0f;
        constexpr float _two_pi = 6.28318530717958647692f;
        float _sun_angle = 0.0f;

        const Ecs::Entity _light = e->world_->createEntity();

        e->world_->add<Renderer::Transform>(_light, Renderer::Transform{
            .position = {
                .x = _center_x + _sun_orbit_radius,
                .y = _center_y + _sun_vertical_offset,
                .z = _center_z,
            },
            .rotation = {},
            .scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
        });

        e->world_->add<Renderer::LightComponent>(_light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {.x = 1.0f, .y = 0.96f, .z = 0.90f},
            .intensity = _sun_orbit_radius * _sun_orbit_radius * 4.0f,
        });

        for (std::size_t i = 0; i < Models::partCount(_model); ++i)
        {
            const Models::ModelPart *part = Models::part(_model, i);
            if (!part) continue;

            const Ecs::Entity _entity = e->world_->createEntity();

            e->world_->add<Renderer::Transform>(
                _entity,
                Renderer::Transform{}
            );

            e->world_->add<Renderer::MeshComponent>(
                _entity,
                Renderer::MeshComponent{part->mesh, part->material}
            );

            e->world_->add<Renderer::RenderableComponent>(
                _entity,
                Renderer::RenderableComponent{true}
            );
        }

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

            _sun_angle = std::fmod(
                _sun_angle + frame_delta * (_two_pi / _sun_orbit_seconds),
                _two_pi
            );

            if (Renderer::Transform *sun = e->world_->get<Renderer::Transform>(_light))
            {
                sun->position.x = _center_x + std::cos(_sun_angle) * _sun_orbit_radius;
                sun->position.y = _center_y + _sun_vertical_offset;
                sun->position.z = _center_z + std::sin(_sun_angle) * _sun_orbit_radius;
                e->world_->markChanged();
            }

            e->animation_system_->update(*e->world_, frame_delta);
            e->camera_controller_->update(*e->world_, frame_delta);

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
