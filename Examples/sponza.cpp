#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
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
    using RenderTechnique = UI::RendererChoice;

    Renderer::Rasterizer *rasterizer_ = new Renderer::Rasterizer();
    Renderer::RayTracer *ray_tracer_ = new Renderer::RayTracer();
    Renderer::PathTracer *path_tracer_ = new Renderer::PathTracer();
    Camera::Controller *camera_controller_ = new Camera::Controller();
    Animation::System *animation_system_ = new Animation::System();
    Ecs::World *world_ = new Ecs::World();
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;

    RenderTechnique technique_ = RenderTechnique::Rasterizer;
    RenderTechnique active_trace_ = RenderTechnique::Rasterizer;
    bool rasterizer_ready_ = false;
    bool ray_tracer_ready_ = false;
    bool path_tracer_ready_ = false;
    bool trace_surface_active_ = false;

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

    bool techniqueReady(RenderTechnique technique) const
    {
        switch (technique)
        {
            case RenderTechnique::Rasterizer: return rasterizer_ready_;
            case RenderTechnique::RayTracer: return ray_tracer_ready_;
            case RenderTechnique::PathTracer: return path_tracer_ready_;
        }
        return false;
    }

    void cycleTechnique()
    {
        RenderTechnique next = technique_;
        for (int i = 0; i < 3; ++i)
        {
            switch (next)
            {
                case RenderTechnique::Rasterizer: next = RenderTechnique::RayTracer; break;
                case RenderTechnique::RayTracer: next = RenderTechnique::PathTracer; break;
                case RenderTechnique::PathTracer: next = RenderTechnique::Rasterizer; break;
            }

            if (techniqueReady(next))
            {
                technique_ = next;
                return;
            }
        }
    }

    bool useRayTracer(bool camera_moving) const
    {
        (void)camera_moving;
        return technique_ == RenderTechnique::RayTracer && ray_tracer_ready_;
    }

    bool usePathTracer(bool camera_moving) const
    {
        (void)camera_moving;
        return technique_ == RenderTechnique::PathTracer && path_tracer_ready_;
    }

    const char *techniqueLabel(bool camera_moving) const
    {
        (void)camera_moving;
        return UI::rendererName(technique_);
    }

    bool setTraceSurface(bool active)
    {
        if (!active)
        {
            if (!trace_surface_active_)
            {
                active_trace_ = RenderTechnique::Rasterizer;
                return true;
            }

#ifdef __APPLE__
            if (Metal.isCreated()) Metal.waitIdle();
            lwmglSurfaceDetach();
#endif
            trace_surface_active_ = false;
            active_trace_ = RenderTechnique::Rasterizer;
            return true;
        }

        if (technique_ == RenderTechnique::Rasterizer || !techniqueReady(technique_))
            return false;

#ifdef __APPLE__
        if (!trace_surface_active_)
        {
            if (lwmglSurfaceAttach(Display.getNativeWindow()) != 0)
            {
                std::fprintf(stderr, "[LOG]: failed to attach Metal surface: %s\n", lwmglGetLastError());
                return false;
            }
            trace_surface_active_ = true;
        }
#else
        trace_surface_active_ = true;
#endif

        if (active_trace_ == technique_)
            return true;

        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);

        if (technique_ == RenderTechnique::RayTracer)
        {
            ray_tracer_->resize(width, height);
            ray_tracer_ready_ = ray_tracer_->initialized();
        }
        else
        {
            path_tracer_->resize(width, height);
            path_tracer_ready_ = path_tracer_->initialized();
        }

        if (!techniqueReady(technique_))
        {
#ifdef __APPLE__
            if (Metal.isCreated()) Metal.waitIdle();
            lwmglSurfaceDetach();
#endif
            trace_surface_active_ = false;
            active_trace_ = RenderTechnique::Rasterizer;
            return false;
        }

        active_trace_ = technique_;
        return true;
    }

    bool prepareTechnique()
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!techniqueReady(technique_))
            {
                cycleTechnique();
                continue;
            }

            if (technique_ == RenderTechnique::Rasterizer)
                return setTraceSurface(false);

            if (setTraceSurface(true))
                return true;

            cycleTechnique();
        }

        setTraceSurface(false);
        return false;
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

        if (!UI::init())
            std::fprintf(stderr, "[UI]: initialization failed\n");

        rasterizer_ready_ = rasterizer_->init();
        ray_tracer_ready_ = ray_tracer_->init();
        path_tracer_ready_ = path_tracer_->init();
        trace_surface_active_ = ray_tracer_ready_ || path_tracer_ready_;

        if (rasterizer_ready_)
        {
            technique_ = RenderTechnique::Rasterizer;
            setTraceSurface(false);
        }
        else if (ray_tracer_ready_)
        {
            technique_ = RenderTechnique::RayTracer;
        }
        else if (path_tracer_ready_)
        {
            technique_ = RenderTechnique::PathTracer;
        }

        if (!rasterizer_ready_ && !ray_tracer_ready_ && !path_tracer_ready_)
            std::fprintf(stderr, "[LOG]: no renderer could be initialized\n");
    }

    ~Example()
    {
        UI::shutdown();
        setTraceSurface(false);
        path_tracer_->shutdown();
        ray_tracer_->shutdown();
        rasterizer_->shutdown();
        delete path_tracer_;
        delete ray_tracer_;
        delete rasterizer_;
        path_tracer_ = nullptr;
        ray_tracer_ = nullptr;
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

        Example *e = new Example("Sponza", {1280, 720});
        if (!e->rasterizer_ready_ && !e->ray_tracer_ready_ && !e->path_tracer_ready_)
        {
            delete e;
            return 2;
        }

        int _framebuffer_width  = std::max(Display.getWidth(),  1),
            _framebuffer_height = std::max(Display.getHeight(), 1);

        if (e->rasterizer_ready_)
            e->rasterizer_->resize(_framebuffer_width, _framebuffer_height);

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
        const Models::ModelHandle _model = Models::load("Assets/Sponza/sponza.obj", &_error);

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
        const float _scene_radius = std::max({_extent_x, _extent_y, _extent_z});

        const Ecs::Entity _light = e->world_->createEntity();

        e->world_->add<Renderer::Transform>(_light, Renderer::Transform{
            .position = {
                .x = _valid_bounds ? (_min_x + _max_x)  * 0.5f  : 0.0f,
                .y = _valid_bounds ? _min_y + _extent_y * 0.78f : 8.0f,
                .z = _valid_bounds ? (_min_z + _max_z)  * 0.5f  : 0.0f,
            },
            .rotation = {},
            .scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
        });

        e->world_->add<Renderer::LightComponent>(_light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {.x = 1.0f, .y = 0.96f, .z = 0.90f},
            .intensity = _scene_radius * _scene_radius * 3.0f,
        });

        const Ecs::Entity _gi = e->world_->createEntity();
        e->world_->add<Renderer::GlobalIlluminationComponent>(
            _gi,
            Renderer::GlobalIlluminationComponent{
                .enabled = true,
                .intensity = 1.0f,
                .bounces = 2,
            }
        );

        std::size_t _triangle_count = 0u;
        for (std::size_t i = 0; i < Models::partCount(_model); ++i)
        {
            const Models::ModelPart *part = Models::part(_model, i);
            if (!part) continue;

            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (mesh) _triangle_count += mesh->indices.size() / 3u;

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

        const Ecs::Entity _stats = Font::screen(
            *e->world_,
            "",
            {12.0f, 140.0f},
            2.0f
        );

        if (e->rasterizer_ready_)
        {
            if (e->ray_tracer_ready_)
            {
                e->technique_ = RenderTechnique::RayTracer;
                if (e->setTraceSurface(true))
                    e->ray_tracer_->render(*e->world_);
            }
            if (e->path_tracer_ready_)
            {
                e->technique_ = RenderTechnique::PathTracer;
                if (e->setTraceSurface(true))
                    e->path_tracer_->render(*e->world_);
            }
            e->technique_ = RenderTechnique::Rasterizer;
            e->setTraceSurface(false);
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

            UI::beginFrame();
            ImGui::SetNextWindowBgAlpha(0.92f);
            UI::rendererSelector(
                e->technique_,
                UI::RendererAvailability{
                    .rasterizer = e->rasterizer_ready_,
                    .ray_tracer = e->ray_tracer_ready_,
                    .path_tracer = e->path_tracer_ready_,
                }
            );

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
            if (enter_down && !_enter_down && !UI::wantsKeyboard())
                e->cycleTechnique();
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
            if (!UI::wantsMouse() && !UI::wantsKeyboard())
                e->camera_controller_->update(*e->world_, frame_delta);

            bool camera_moving = false;
            if (had_camera_before)
            {
                if (const Renderer::Transform *camera = e->world_->get<Renderer::Transform>(e->camera_))
                    camera_moving = cameraChanged(camera_before, *camera);
            }

            e->prepareTechnique();

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
                if (e->trace_surface_active_)
                {
                    if (e->useRayTracer(camera_moving)) e->ray_tracer_->resize(width, height);
                    if (e->usePathTracer(camera_moving)) e->path_tracer_->resize(width, height);
                }
            }

            if (e->useRayTracer(camera_moving) && e->trace_surface_active_)
                e->ray_tracer_->render(*e->world_);
            else if (e->usePathTracer(camera_moving) && e->trace_surface_active_)
                e->path_tracer_->render(*e->world_);
            else if (e->rasterizer_ready_)
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
