#include "Scenes/Earth.hpp"
#include "Scenes/Manager.hpp"
#include "Scenes/Sponza.hpp"

#include "Font.hpp"
#include "Renderer/Debug/Debug.hpp"
#include "Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

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
#include <memory>
#include <string>

namespace Game {

class Application {
public:
    int run()
    {
        if (!init()) return 2;
        if (!loadScene(0u)) return 3;

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();

        while (!Display.isCloseRequested()) {
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            updateMouseMode();

            std::size_t requested_scene = scenes_.currentIndex();
            if (UI::beginFrame()) {
                UI::SceneSelection selection {
                    .names = scenes_.names(),
                    .count = scenes_.count(),
                    .selected = requested_scene,
                };
                UI::enginePanels(
                    *world_,
                    ray_tracer_,
                    path_tracer_,
                    technique_,
                    availability(),
                    selection
                );
                requested_scene = selection.selected;
            }

            updateTechniqueKey();

            if (requested_scene != scenes_.currentIndex() && !loadScene(requested_scene)) {
                std::fprintf(stderr, "[GAME]: keeping scene %s\n", currentSceneName());
            }

            const auto now = Clock::now();
            const float delta_seconds = std::chrono::duration<float>(now - previous).count();
            previous = now;
            const float frame_delta = std::min(delta_seconds, 0.1f);

            if (Scenes::Scene *scene = scenes_.current())
                scene->update(*world_, frame_delta);
            animation_system_.update(*world_, frame_delta);
            if (!UI::wantsMouse() && !UI::wantsKeyboard())
                camera_controller_.update(*world_, frame_delta);

            if (!prepareTechnique()) continue;
            updateStats(delta_seconds);
            resizeIfNeeded();
            render();
        }

        return 0;
    }

    ~Application()
    {
        shutdown();
    }

private:
    using RenderTechnique = UI::RendererChoice;

    bool init()
    {
        if (started_) return true;

        lwcglInstallFastRuntime();
#ifdef __APPLE__
        lwcglSetContextVersion(2, 1);
        lwcglSetContextProfile(LWCGL_CONTEXT_ANY_PROFILE);
#else
        lwcglSetContextVersion(4, 3);
        lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

        Display.setDisplayMode(new DisplayMode(1280, 720));
        Display.create();
        Display.setTitle("GAME");
        started_ = true;

        Keyboard.create();
        Mouse.create();
        Mouse.setGrabbed(LWCGL_TRUE);
        Mouse.getDX();
        Mouse.getDY();

        if (!UI::init())
            std::fprintf(stderr, "[UI]: initialization failed\n");

        rasterizer_ready_ = rasterizer_.init();
        ray_tracer_ready_ = ray_tracer_.init();
        path_tracer_ready_ = path_tracer_.init();
#ifdef __APPLE__
        trace_surface_active_ = Metal.isCreated() != 0;
#else
        trace_surface_active_ = ray_tracer_ready_ || path_tracer_ready_;
#endif

        if (rasterizer_ready_) {
            technique_ = RenderTechnique::Rasterizer;
            setTraceSurface(false);
        } else if (ray_tracer_ready_) {
            technique_ = RenderTechnique::RayTracer;
            if (!setTraceSurface(true)) ray_tracer_ready_ = false;
        } else if (path_tracer_ready_) {
            technique_ = RenderTechnique::PathTracer;
            if (!setTraceSurface(true)) path_tracer_ready_ = false;
        }

        if (!rasterizer_ready_ && !ray_tracer_ready_ && !path_tracer_ready_) {
            std::fprintf(stderr, "[GAME]: no renderer could be initialized\n");
            return false;
        }

        framebuffer_width_ = std::max(Display.getWidth(), 1);
        framebuffer_height_ = std::max(Display.getHeight(), 1);
        if (rasterizer_ready_)
            rasterizer_.resize(framebuffer_width_, framebuffer_height_);

        scenes_.add<Scenes::Sponza>();
        scenes_.add<Scenes::Earth>();
        return true;
    }

    void shutdown()
    {
        if (!started_) return;

        UI::shutdown();
        Renderer::Debug::shutdown();
        setTraceSurface(false);
        path_tracer_.shutdown();
        ray_tracer_.shutdown();
        rasterizer_.shutdown();
        Renderer::GlobalIllumination::reset();

        world_.reset();
        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();

        started_ = false;
        rasterizer_ready_ = false;
        ray_tracer_ready_ = false;
        path_tracer_ready_ = false;
        trace_surface_active_ = false;
    }

    UI::RendererAvailability availability() const
    {
        return {
            .rasterizer = rasterizer_ready_,
            .ray_tracer = ray_tracer_ready_,
            .path_tracer = path_tracer_ready_,
        };
    }

    Scenes::Renderers renderers()
    {
        return {rasterizer_, ray_tracer_, path_tracer_};
    }

    bool loadScene(std::size_t index)
    {
        Scenes::Scene *scene = scenes_.at(index);
        if (!scene) return false;

        auto next_world = std::make_unique<Ecs::World>();
        std::string error;
        if (!scene->load(*next_world, error)) {
            std::fprintf(
                stderr,
                "[GAME]: failed to load scene %s: %s\n",
                scene->name(),
                error.empty() ? "unknown error" : error.c_str()
            );
            return false;
        }

        if (scene->camera() == Ecs::INVALID_ENTITY || !next_world->alive(scene->camera())) {
            std::fprintf(stderr, "[GAME]: scene %s did not create a valid camera\n", scene->name());
            return false;
        }

        Scenes::Renderers scene_renderers = renderers();
        scene->configure(scene_renderers);

        Renderer::GlobalIllumination::reset();
        Renderer::Debug::clear();
        world_ = std::move(next_world);
        scenes_.activate(index);

        stats_ = Font::screen(
            *world_,
            "",
            {12.0f, 140.0f},
            2.0f
        );

        Display.setTitle(scene->name());
        return true;
    }

    const char *currentSceneName() const
    {
        const Scenes::Scene *scene = scenes_.current();
        return scene ? scene->name() : "No Scene";
    }

    bool techniqueReady(RenderTechnique technique) const
    {
        switch (technique) {
            case RenderTechnique::Rasterizer: return rasterizer_ready_;
            case RenderTechnique::RayTracer: return ray_tracer_ready_;
            case RenderTechnique::PathTracer: return path_tracer_ready_;
        }
        return false;
    }

    void cycleTechnique()
    {
        RenderTechnique next = technique_;
        for (int attempt = 0; attempt < 3; ++attempt) {
            switch (next) {
                case RenderTechnique::Rasterizer: next = RenderTechnique::RayTracer; break;
                case RenderTechnique::RayTracer: next = RenderTechnique::PathTracer; break;
                case RenderTechnique::PathTracer: next = RenderTechnique::Rasterizer; break;
            }
            if (techniqueReady(next)) {
                technique_ = next;
                return;
            }
        }
    }

    bool setTraceSurface(bool active)
    {
        if (!active) {
            if (!trace_surface_active_) {
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
        if (!trace_surface_active_) {
            if (lwmglSurfaceAttach(Display.getNativeWindow()) != 0) {
                std::fprintf(stderr, "[GAME]: failed to attach Metal surface: %s\n", lwmglGetLastError());
                return false;
            }
            trace_surface_active_ = true;
        }
#else
        trace_surface_active_ = true;
#endif

        if (active_trace_ == technique_) return true;

        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);
        if (technique_ == RenderTechnique::RayTracer) {
            ray_tracer_.resize(width, height);
            ray_tracer_ready_ = ray_tracer_.initialized();
        } else {
            path_tracer_.resize(width, height);
            path_tracer_ready_ = path_tracer_.initialized();
        }

        if (!techniqueReady(technique_)) {
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
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (!techniqueReady(technique_)) {
                cycleTechnique();
                continue;
            }
            if (technique_ == RenderTechnique::Rasterizer)
                return setTraceSurface(false);
            if (setTraceSurface(true)) return true;
            cycleTechnique();
        }

        setTraceSurface(false);
        return false;
    }

    void updateMouseMode()
    {
        const bool tab_down = Keyboard.isKeyDown(Keyboard.KEY_TAB);
        if (tab_down && !tab_down_) {
            const bool grabbed = Mouse.isGrabbed() != LWCGL_FALSE;
            Mouse.setGrabbed(grabbed ? LWCGL_FALSE : LWCGL_TRUE);
            Mouse.getDX();
            Mouse.getDY();
        }
        tab_down_ = tab_down;
    }

    void updateTechniqueKey()
    {
        const bool enter_down = Keyboard.isKeyDown(Keyboard.KEY_RETURN);
        if (enter_down && !enter_down_ && !UI::wantsKeyboard())
            cycleTechnique();
        enter_down_ = enter_down;
    }

    void updateStats(float delta_seconds)
    {
        if (!world_ || stats_ == Ecs::INVALID_ENTITY) return;
        Font::TextComponent *stats = world_->get<Font::TextComponent>(stats_);
        if (!stats) return;

        const long fps = delta_seconds > 1.0e-6f
            ? std::lround(1.0 / static_cast<double>(delta_seconds))
            : 0L;
        const Scenes::Scene *scene = scenes_.current();
        const std::size_t triangles = scene ? scene->triangleCount() : 0u;

        stats->text =
            "Scene: " + std::string(currentSceneName()) +
            "\nTechnique: " + std::string(UI::rendererName(technique_)) +
            "\nFPS: " + std::to_string(fps) +
            "\nTriangles: " + std::to_string(triangles);
    }

    void resizeIfNeeded()
    {
        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);
        if (width == framebuffer_width_ && height == framebuffer_height_) return;

        framebuffer_width_ = width;
        framebuffer_height_ = height;
        if (rasterizer_ready_) rasterizer_.resize(width, height);
        if (trace_surface_active_) {
            if (technique_ == RenderTechnique::RayTracer && ray_tracer_ready_)
                ray_tracer_.resize(width, height);
            if (technique_ == RenderTechnique::PathTracer && path_tracer_ready_)
                path_tracer_.resize(width, height);
        }
    }

    void render()
    {
        if (!world_) return;
        if (technique_ == RenderTechnique::RayTracer && ray_tracer_ready_ && trace_surface_active_) {
            ray_tracer_.render(*world_);
            return;
        }
        if (technique_ == RenderTechnique::PathTracer && path_tracer_ready_ && trace_surface_active_) {
            path_tracer_.render(*world_);
            return;
        }
        if (rasterizer_ready_)
            rasterizer_.render(*world_);
    }

    Renderer::Rasterizer rasterizer_;
    Renderer::RayTracer ray_tracer_;
    Renderer::PathTracer path_tracer_;
    Camera::Controller camera_controller_;
    Animation::System animation_system_;
    std::unique_ptr<Ecs::World> world_;
    Scenes::Manager scenes_;

    Ecs::Entity stats_ = Ecs::INVALID_ENTITY;
    RenderTechnique technique_ = RenderTechnique::Rasterizer;
    RenderTechnique active_trace_ = RenderTechnique::Rasterizer;

    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;
    bool started_ = false;
    bool rasterizer_ready_ = false;
    bool ray_tracer_ready_ = false;
    bool path_tracer_ready_ = false;
    bool trace_surface_active_ = false;
    bool tab_down_ = false;
    bool enter_down_ = false;
};

} // namespace Game

int main()
{
    Game::Application application;
    return application.run();
}