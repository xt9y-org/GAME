#include "earth.hpp"

#include "Font.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/Rasterizer/Rasterizer.hpp"
#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Core/Texture.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"

#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>
#ifdef __APPLE__
#include <lwmgl/lwmgl.h>
extern "C" int lwmglSurfaceAttach(void *nativeWindow);
extern "C" void lwmglSurfaceDetach(void);
#endif

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
    enum class RenderTechnique
    {
        Rasterizer,
        PathTracer,
    };

    Renderer::Rasterizer *rasterizer_ = new Renderer::Rasterizer();
    Renderer::PathTracer *path_tracer_ = new Renderer::PathTracer();
    Camera::Controller *camera_controller_ = new Camera::Controller();
    Animation::System *animation_system_ = new Animation::System();
    Ecs::World *world_ = new Ecs::World();
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;

    RenderTechnique technique_ = RenderTechnique::Rasterizer;
    bool rasterizer_ready_ = false;
    bool path_tracer_ready_ = false;
    bool metal_surface_active_ = false;

    static bool cameraChanged(
        const Renderer::Transform& before,
        const Renderer::Transform& after)
    {
        constexpr float epsilon = 1.0e-6f;
        const auto changed = [](float a, float b) {
            return std::fabs(a - b) > epsilon;
        };

        return
            changed(before.position.x, after.position.x) ||
            changed(before.position.y, after.position.y) ||
            changed(before.position.z, after.position.z) ||
            changed(before.rotation.x, after.rotation.x) ||
            changed(before.rotation.y, after.rotation.y) ||
            changed(before.rotation.z, after.rotation.z);
    }

    bool usePathTracer(bool camera_moving) const
    {
        if (technique_ == RenderTechnique::PathTracer && path_tracer_ready_)
        {
            if (!camera_moving || !rasterizer_ready_) return true;
        }
        return !rasterizer_ready_ && path_tracer_ready_;
    }

    const char *techniqueLabel(bool camera_moving) const
    {
        if (usePathTracer(camera_moving)) return "PathTracer";
        if (technique_ == RenderTechnique::PathTracer && camera_moving)
            return "Rasterizer (moving)";
        return "Rasterizer";
    }

    bool setPathTracerSurface(bool active)
    {
        if (!path_tracer_ready_) return false;
        if (metal_surface_active_ == active) return true;

#ifdef __APPLE__
        if (active)
        {
            if (lwmglSurfaceAttach(Display.getNativeWindow()) != 0)
            {
                std::fprintf(stderr, "[LOG]: failed to attach Metal surface: %s\n", lwmglGetLastError());
                return false;
            }

            metal_surface_active_ = true;
            const int width = std::max(Display.getWidth(), 1);
            const int height = std::max(Display.getHeight(), 1);
            path_tracer_->resize(width, height);
            path_tracer_ready_ = path_tracer_->initialized();
            if (!path_tracer_ready_)
            {
                lwmglSurfaceDetach();
                metal_surface_active_ = false;
                return false;
            }
            return true;
        }

        if (Metal.isCreated()) Metal.waitIdle();
        lwmglSurfaceDetach();
        metal_surface_active_ = false;
        return true;
#else
        metal_surface_active_ = active;
        return true;
#endif
    }

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
        Mouse.setGrabbed(LWCGL_TRUE);
        Mouse.getDX();
        Mouse.getDY();

        rasterizer_ready_ = rasterizer_->init();
        path_tracer_ready_ = path_tracer_->init();
        metal_surface_active_ = path_tracer_ready_;

        if (path_tracer_ready_)
        {
            Renderer::PathTracerSettings& settings = path_tracer_->settings();
            settings.resolution_divisor = 2;
            settings.samples_per_frame = 2;
            settings.exposure = 1.05f;
        }

        if (rasterizer_ready_ && path_tracer_ready_)
            setPathTracerSurface(false);
        else if (!rasterizer_ready_ && path_tracer_ready_)
            technique_ = RenderTechnique::PathTracer;

        if (!rasterizer_ready_ && !path_tracer_ready_)
            std::fprintf(stderr, "[LOG]: no renderer could be initialized\n");
    }

    ~Example()
    {
        path_tracer_->shutdown();
        rasterizer_->shutdown();
        delete path_tracer_;
        delete rasterizer_;
        path_tracer_ = nullptr;
        rasterizer_ = nullptr;

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
        if (!e->rasterizer_ready_ && !e->path_tracer_ready_)
        {
            delete e;
            return 2;
        }

        int _framebuffer_width  = std::max(Display.getWidth(),  1),
            _framebuffer_height = std::max(Display.getHeight(), 1);

        if (e->rasterizer_ready_)
            e->rasterizer_->resize(_framebuffer_width, _framebuffer_height);
        if (e->path_tracer_ready_ && e->metal_surface_active_)
            e->path_tracer_->resize(_framebuffer_width, _framebuffer_height);

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

        const Ecs::Entity _gi = e->world_->createEntity();
        e->world_->add<Renderer::GlobalIlluminationComponent>(
            _gi,
            Renderer::GlobalIlluminationComponent{
                .enabled = true,
                .intensity = 1.0f,
                .bounces = 1,
            }
        );

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

        if (e->rasterizer_ready_ && e->path_tracer_ready_)
        {
            if (e->setPathTracerSurface(true))
                e->path_tracer_->render(*e->world_);
            e->setPathTracerSurface(false);
            e->rasterizer_->render(*e->world_);
        }

        using Clock = std::chrono::steady_clock;
        auto _previous = Clock::now();
        bool _tab_down = false;
        bool _enter_down = false;

        while (!Display.isCloseRequested())
        {
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            const bool tab_down = Keyboard.isKeyDown(Keyboard.KEY_TAB);
            if (tab_down && !_tab_down)
            {
                const bool grabbed = Mouse.isGrabbed() != LWCGL_FALSE;
                Mouse.setGrabbed(grabbed ? LWCGL_FALSE : LWCGL_TRUE);
                Mouse.getDX();
                Mouse.getDY();
            }
            _tab_down = tab_down;

            const bool enter_down = Keyboard.isKeyDown(Keyboard.KEY_RETURN);
            if (enter_down && !_enter_down)
            {
                if (e->technique_ == RenderTechnique::Rasterizer && e->path_tracer_ready_)
                    e->technique_ = RenderTechnique::PathTracer;
                else if (e->technique_ == RenderTechnique::PathTracer && e->rasterizer_ready_)
                    e->technique_ = RenderTechnique::Rasterizer;
            }
            _enter_down = enter_down;

            const auto now = Clock::now();
            const float delta_seconds = std::chrono::duration<float>(now - _previous).count();
            _previous = now;
            const float frame_delta = std::min(delta_seconds, 0.1f);

            Renderer::Transform camera_before{};
            bool had_camera_before = false;
            if (const Renderer::Transform *camera = e->world_->get<Renderer::Transform>(e->camera_))
            {
                camera_before = *camera;
                had_camera_before = true;
            }

            e->animation_system_->update(*e->world_, frame_delta);
            e->camera_controller_->update(*e->world_, frame_delta);

            bool camera_moving = false;
            if (had_camera_before)
            {
                if (const Renderer::Transform *camera = e->world_->get<Renderer::Transform>(e->camera_))
                    camera_moving = cameraChanged(camera_before, *camera);
            }

            const bool wants_path_tracer = e->usePathTracer(camera_moving);
            if (wants_path_tracer)
            {
                if (!e->setPathTracerSurface(true))
                    e->technique_ = RenderTechnique::Rasterizer;
            }
            else
            {
                e->setPathTracerSurface(false);
            }

            if (Font::TextComponent *stats = e->world_->get<Font::TextComponent>(_stats))
            {
                const long fps = delta_seconds > 1.0e-6f
                    ? std::lround(1.0 / static_cast<double>(delta_seconds))
                    : 0L;
                stats->text =
                    "Technique: " + std::string(e->techniqueLabel(camera_moving)) +
                    "\nFPS: " + std::to_string(fps) +
                    "\nTriangles: " + std::to_string(_triangle_count);
            }

            const int width  = std::max(Display.getWidth(), 1);
            const int height = std::max(Display.getHeight(), 1);

            if ((width != _framebuffer_width) || height != _framebuffer_height)
            {
                _framebuffer_height = height;
                _framebuffer_width  = width;
                if (e->rasterizer_ready_) e->rasterizer_->resize(width, height);
                if (e->path_tracer_ready_ && e->metal_surface_active_)
                    e->path_tracer_->resize(width, height);
            }

            if (e->usePathTracer(camera_moving) && e->metal_surface_active_)
                e->path_tracer_->render(*e->world_);
            else
                e->rasterizer_->render(*e->world_);
        }

        delete e;
        return 0;
    }
};

int main(int argc, char **argv)
{
    return Example::run(argc, argv);
}
