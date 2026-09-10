#include "Scenes/Earth.hpp"
#include "Scenes/Manager.hpp"
#include "Scenes/Sponza.hpp"
#include "Tests/RendererCheck.hpp"
#include "UI/Interface.hpp"

#include "Font.hpp"
#include "Renderer/Debug/Debug.hpp"
#include "Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Renderer/Scenes/SceneCache.hpp"
#include "Renderer/Visibility/Visibility.hpp"
#include "Sources/Animation/Animation.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

namespace Game {

class Application {
public:
    ~Application()
    {
        shutdown();
    }

    int run()
    {
        if (!init()) return 2;
        if (!loadScene(0u)) return 3;

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();
        std::uint64_t frame_index = 0u;

        while (!Display.isCloseRequested()) {
            const auto frame_started = Clock::now();
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            if (!renderer_check_.active()) updateMouseMode();

            std::size_t requested_scene = scenes_.currentIndex();
            const bool ui_visible = !renderer_check_.active() && ::UI::beginFrame();
            if (ui_visible && world_) {
                requested_scene = interface_.draw(
                    *world_,
                    scenes_,
                    renderers_,
                    inspector_
                );
            }

            updateRendererKey();

            if (requested_scene != scenes_.currentIndex() && !loadScene(requested_scene)) {
                std::fprintf(stderr, "[GAME]: keeping scene %s\n", currentSceneName());
            }

            const auto now = Clock::now();
            const float delta_seconds = std::chrono::duration<float>(now - previous).count();
            previous = now;
            const float frame_delta = std::min(delta_seconds, 0.1f);

            const bool frozen = inspector_.frozen();
            Renderer::GlobalIllumination::setPaused(frozen);

            if (!frozen) {
                if (Scenes::Scene *scene = scenes_.current())
                    scene->update(*world_, frame_delta);
                animation_system_.update(*world_, frame_delta);
            }

            if (!renderer_check_.active() && !::UI::wantsMouse() && !::UI::wantsKeyboard())
                camera_controller_.update(*world_, frame_delta);

            if (!renderer_check_.active()) updateStats(delta_seconds);
            resizeIfNeeded();
            renderers_.render(*world_);

            if (renderer_check_.active()) {
                if (auto *rasterizer = dynamic_cast<Renderer::Rasterizer *>(renderers_.active()))
                    renderer_check_.record(*rasterizer);
            }

            const auto frame_finished = Clock::now();
            renderer_check_.metric(
                "frame_ms",
                std::chrono::duration<double, std::milli>(frame_finished - frame_started).count()
            );

            if (renderer_check_.captureDue(frame_index)) {
                const Renderer::Manager::Entry *active = renderers_.activeEntry();
                const bool rasterizer = active && active->name == "Rasterizer";
                if (!rasterizer || !renderer_check_.captureOpenGL(framebuffer_width_, framebuffer_height_)) {
                    std::fprintf(stderr, "[RendererCheck]: capture failed\n");
                    return 4;
                }
            }

            if (renderer_check_.lastFrame(frame_index)) break;
            ++frame_index;
        }

        return 0;
    }

private:
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

        configureEngine();

        if (!::UI::init()) {
            std::fprintf(stderr, "[UI]: initialization failed\n");
        } else {
            configureUi();
        }

        configureRenderers();
        if (!renderers_.initialize()) {
            std::fprintf(stderr, "[GAME]: no renderer could be initialized\n");
            return false;
        }

        framebuffer_width_ = std::max(Display.getWidth(), 1);
        framebuffer_height_ = std::max(Display.getHeight(), 1);
        renderers_.resize(framebuffer_width_, framebuffer_height_);

        scenes_.add<Scenes::Sponza>();
        scenes_.add<Scenes::Earth>();
        return true;
    }

    void configureEngine()
    {
        camera_controller_.setSpeed(100.0f);
        camera_controller_.setSprintMultiplier(10.0f);
        camera_controller_.setMouseSensitivity(0.12f);
        camera_controller_.setPitchRange(-89.0f, 89.0f);

        Font::configureAtlas("Assets/Font/font.png", 16u, 16u, 8.0f);

        Renderer::Scenes::SceneCache::setLeafSize(8u);
        Renderer::Scenes::SceneCache::setMaximumTriangles(1000000u);
        Renderer::Scenes::SceneCache::setOpacityCutoff(0.5f);
        Renderer::Scenes::SceneCache::setAlphaThreshold(250u);

        Renderer::GlobalIllumination::setRaysPerProbe(48u);
        Renderer::GlobalIllumination::setProbeBudgetPerFrame(16u);
        Renderer::GlobalIllumination::setProbeDimensionRange(3u, 8u);
        Renderer::GlobalIllumination::setBoundsMargin(0.05f, 0.25f);
        Renderer::GlobalIllumination::setRayEpsilon(0.0025f);
        Renderer::GlobalIllumination::setMaximumBounces(4u);
        Renderer::GlobalIllumination::setMaximumPhotonCount(262144u);
        Renderer::GlobalIllumination::setPaused(false);

        Renderer::Visibility::system().setFarDistance(10000.0f);

        inspector_.setShowBvh(false);
        inspector_.setShowViewport(false);
        inspector_.setBvhLevel(2);
        inspector_.setOverlayOpacity(0.80f);
        inspector_.setBvhColor({0.20f, 0.52f, 1.00f, 1.00f});
        inspector_.setHighlightColor({1.00f, 0.82f, 0.16f, 1.00f});
        inspector_.setPlayerColor({1.00f, 0.12f, 0.12f, 1.00f});
        inspector_.setPlayerHeight(1.80f);
        inspector_.setCameraMarkerSize(0.08f);
    }

    void configureUi()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = "imgui.ini";
        io.LogFilename = nullptr;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontScaleMain = 0.82f;
        style.ScaleAllSizes(0.82f);
        style.FrameRounding = 0.0f;
        style.WindowRounding = 4.0f;

        interface_.setApproximationWindow(8.0f, 8.0f, 350.0f, 330.0f);
        interface_.setSceneWindow(370.0f, 8.0f, 260.0f, 125.0f);
        interface_.setInformationWindow(370.0f, 145.0f, 260.0f, 430.0f);
        interface_.setDebugWindow(642.0f, 8.0f, 360.0f, 610.0f);
        interface_.setTooltip(4.0f, 3.0f, 220.0f);
        interface_.setControlWidth(150.0f);
    }

    void configureRenderers()
    {
        auto& rasterizer = renderers_.add<Renderer::Rasterizer>("Rasterizer");
        rasterizer.setEnabled(true);
        rasterizer.setViewportCulling(true);
        rasterizer.setShadowResolution(2048);
        rasterizer.setShadowResolutionDivisor(1);
        rasterizer.setFallbackShadowResolution(512);
        rasterizer.setMinimumShadowResolution(64);
        rasterizer.setShadowNearPlane(0.05f);
        rasterizer.setShadowFarScale(1.05f);
        rasterizer.setLightingResolutionDivisor(1);
        rasterizer.setDepthAwareUpscaling(true);
        rasterizer.setTemporalUpscaling(false);
        rasterizer.setTemporalUpscalingWeight(0.85f);
        rasterizer.setUpscalingDepthThreshold(0.02f);
        rasterizer.setHorizonGiEnabled(false);
        rasterizer.setHorizonGiResolutionDivisor(2);
        rasterizer.setHorizonGiDirections(4);
        rasterizer.setHorizonGiSteps(6);
        rasterizer.setHorizonGiRadius(1.5f);
        rasterizer.setHorizonGiThickness(0.15f);
        rasterizer.setHorizonGiAoStrength(1.0f);
        rasterizer.setHorizonGiIndirectStrength(0.35f);
        rasterizer.setHorizonGiTemporalFilter(true);
        rasterizer.setHorizonGiTemporalWeight(0.85f);
        rasterizer.setClearColor({0.035f, 0.035f, 0.045f, 1.0f});
        renderer_check_.configure(rasterizer);

        auto& ray_tracer = renderers_.add<Renderer::RayTracer>("Ray Tracer");
        ray_tracer.setEnabled(true);
        ray_tracer.setResolutionDivisor(4);
        ray_tracer.setExposure(1.05f);

        auto& path_tracer = renderers_.add<Renderer::PathTracer>("Path Tracer");
        path_tracer.setEnabled(true);
        path_tracer.setResolutionDivisor(2);
        path_tracer.setSamplesPerFrame(1);
        path_tracer.setExposure(1.05f);
        path_tracer.setStationaryPhaseGrid(2);
        path_tracer.setResetPhaseGrid(1);
        path_tracer.setMovingPhaseGrid(4);
        path_tracer.setMovingDepthBlock(2);

        interface_.addIntControl(
            ray_tracer,
            "Resolution Divisor",
            [&ray_tracer] { return ray_tracer.resolutionDivisor(); },
            [&ray_tracer](int value) { ray_tracer.setResolutionDivisor(value); },
            1,
            8,
            "Render the ray tracer at 1/N of the display resolution."
        );
        interface_.addFloatControl(
            ray_tracer,
            "Exposure",
            [&ray_tracer] { return ray_tracer.exposure(); },
            [&ray_tracer](float value) { ray_tracer.setExposure(value); },
            0.1f,
            4.0f,
            0.01f,
            "Brightness applied when the ray traced image is presented."
        );

        interface_.addIntControl(
            path_tracer,
            "Resolution Divisor",
            [&path_tracer] { return path_tracer.resolutionDivisor(); },
            [&path_tracer](int value) { path_tracer.setResolutionDivisor(value); },
            1,
            8,
            "Render the path tracer at 1/N of the display resolution."
        );
        interface_.addIntControl(
            path_tracer,
            "Samples / Frame",
            [&path_tracer] { return path_tracer.samplesPerFrame(); },
            [&path_tracer](int value) { path_tracer.setSamplesPerFrame(value); },
            1,
            16,
            "Number of path tracing samples accumulated per frame."
        );
        interface_.addFloatControl(
            path_tracer,
            "Exposure",
            [&path_tracer] { return path_tracer.exposure(); },
            [&path_tracer](float value) { path_tracer.setExposure(value); },
            0.1f,
            4.0f,
            0.01f,
            "Brightness applied when the path traced image is presented."
        );
    }

    void shutdown()
    {
        if (!started_) return;

        if (world_ && inspector_.frozen()) inspector_.unfreeze(*world_);
        Renderer::GlobalIllumination::setPaused(false);

        ::UI::shutdown();
        Renderer::Debug::shutdown();
        renderers_.shutdown();
        Renderer::GlobalIllumination::reset();

        world_.reset();
        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();

        started_ = false;
    }

    bool loadScene(std::size_t index)
    {
        Scenes::Scene *scene = scenes_.at(index);
        if (!scene) return false;

        if (world_) {
            if (inspector_.frozen()) inspector_.unfreeze(*world_);
            inspector_.clear(world_.get());
        }
        Renderer::GlobalIllumination::setPaused(false);

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

        Renderer::GlobalIllumination::reset();
        world_ = std::move(next_world);
        scenes_.activate(index);

        stats_ = Font::screen(
            *world_,
            "",
            {12.0f, 12.0f},
            2.0f,
            {1.0f, 1.0f, 1.0f, 1.0f}
        );

        Display.setTitle(scene->name());
        return true;
    }

    const char *currentSceneName() const
    {
        const Scenes::Scene *scene = scenes_.current();
        return scene ? scene->name() : "No Scene";
    }

    void updateRendererKey()
    {
        const bool down = Keyboard.isKeyDown(Keyboard.KEY_RETURN);
        if (down && !renderer_key_down_ && !::UI::wantsKeyboard())
            renderers_.next();
        renderer_key_down_ = down;
    }

    void updateMouseMode()
    {
        const bool down = Keyboard.isKeyDown(Keyboard.KEY_TAB);
        if (down && !tab_down_) {
            const bool grabbed = Mouse.isGrabbed() != LWCGL_FALSE;
            Mouse.setGrabbed(grabbed ? LWCGL_FALSE : LWCGL_TRUE);
            Mouse.getDX();
            Mouse.getDY();
        }
        tab_down_ = down;
    }

    void updateStats(float delta_seconds)
    {
        if (!world_ || stats_ == Ecs::INVALID_ENTITY) return;
        Font::TextComponent *text = world_->get<Font::TextComponent>(stats_);
        if (!text) return;

        const Renderer::Manager::Entry *active = renderers_.activeEntry();
        const char *renderer_name = active ? active->name.c_str() : "None";
        const std::size_t triangles = scenes_.current()
            ? scenes_.current()->triangleCount()
            : 0u;
        const float fps = delta_seconds > 0.0f ? 1.0f / delta_seconds : 0.0f;

        char buffer[256]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Technique: %s\nFPS: %.1f\nTriangles: %zu",
            renderer_name,
            fps,
            triangles
        );
        text->text = buffer;
    }

    void resizeIfNeeded()
    {
        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);
        if (width == framebuffer_width_ && height == framebuffer_height_) return;

        framebuffer_width_ = width;
        framebuffer_height_ = height;
        renderers_.resize(width, height);
    }

    bool started_ = false;
    bool tab_down_ = false;
    bool renderer_key_down_ = false;
    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;

    Renderer::Manager renderers_;
    Camera::Controller camera_controller_;
    Animation::System animation_system_;
    Renderer::Debug::Inspector& inspector_ = Renderer::Debug::inspector();

    Scenes::Manager scenes_;
    UI::Interface interface_;
    std::unique_ptr<Ecs::World> world_;
    Ecs::Entity stats_ = Ecs::INVALID_ENTITY;
    Tests::RendererCheck renderer_check_{};
};

} // namespace Game

int main()
{
    Game::Application application;
    return application.run();
}
