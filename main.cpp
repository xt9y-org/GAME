#include "Scenes/Driving.hpp"
#include "Tests/RendererCheck.hpp"

#include "Renderer/Scenes/SceneCache.hpp"
#include "Renderer/Visibility/Visibility.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

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
        if (!load()) return 3;

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();
        std::uint64_t frame_index = 0u;

        while (!Display.isCloseRequested()) {
            const auto frame_started = Clock::now();
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            if (!renderer_check_.active()) updateDebugKey();

            const bool ui_visible = !renderer_check_.active() && ::UI::beginFrame();
            if (ui_visible) driving_.drawDebug(world_);

            updateRendererKey();

            const auto now = Clock::now();
            const float delta_seconds = std::chrono::duration<float>(now - previous).count();
            previous = now;
            const float frame_delta = std::min(delta_seconds, 0.1f);

            const auto update_started = Clock::now();
            driving_.update(world_, frame_delta);
            const auto update_finished = Clock::now();
            renderer_check_.metric(
                "update_ms",
                std::chrono::duration<double, std::milli>(update_finished - update_started).count()
            );

            resizeIfNeeded();

            const auto render_started = Clock::now();
            renderers_.render(world_);
            const auto render_finished = Clock::now();
            renderer_check_.metric(
                "render_ms",
                std::chrono::duration<double, std::milli>(render_finished - render_started).count()
            );

            driving_.emitMetrics([this](std::string_view name, double value) {
                renderer_check_.metric(name, value);
            });

            renderer_check_.metric(
                "frame_ms",
                std::chrono::duration<double, std::milli>(render_finished - frame_started).count()
            );

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
        Display.setTitle("Driving");
        started_ = true;

        Keyboard.create();
        Mouse.create();
        Mouse.setGrabbed(LWCGL_TRUE);
        Mouse.getDX();
        Mouse.getDY();

        configureEngine();
        configureUi();
        configureRenderers();

        if (!renderers_.initialize()) {
            std::fprintf(stderr, "[Driving]: no renderer could be initialized\n");
            return false;
        }

        framebuffer_width_ = std::max(Display.getWidth(), 1);
        framebuffer_height_ = std::max(Display.getHeight(), 1);
        renderers_.resize(framebuffer_width_, framebuffer_height_);

        if (renderer_check_.active() && !renderers_.activate(renderer_check_.rendererName())) {
            std::fprintf(stderr, "[RendererCheck]: requested renderer unavailable\n");
            return false;
        }

        return true;
    }

    void configureEngine()
    {
        Renderer::Scenes::SceneCache::setLeafSize(8u);
        Renderer::Scenes::SceneCache::setMaximumTriangles(1000000u);
        Renderer::Scenes::SceneCache::setOpacityCutoff(0.5f);
        Renderer::Scenes::SceneCache::setAlphaThreshold(250u);
        Renderer::Visibility::system().setFarDistance(10000.0f);
    }

    void configureUi()
    {
        if (!::UI::init()) {
            std::fprintf(stderr, "[Driving]: ImGui initialization failed\n");
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontScaleMain = 0.86f;
        style.ScaleAllSizes(0.86f);
        style.FrameRounding = 0.0f;
        style.WindowRounding = 3.0f;
    }

    void configureRenderers()
    {
        auto& rasterizer = renderers_.add<Renderer::Rasterizer>("Rasterizer");
        rasterizer.setEnabled(true);
        rasterizer.setViewportCulling(true);
        rasterizer.setShadowResolution(2048);
        rasterizer.setFallbackShadowResolution(512);
        rasterizer.setMinimumShadowResolution(64);
        rasterizer.setShadowNearPlane(0.05f);
        rasterizer.setShadowFarScale(1.05f);
        rasterizer.setClearColor({0.47f, 0.58f, 0.70f, 1.0f});

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
        path_tracer.setMovingDepthBlock(4);
    }

    bool load()
    {
        std::string error;
        if (!driving_.load(world_, error)) {
            std::fprintf(
                stderr,
                "[Driving]: failed to load: %s\n",
                error.empty() ? "unknown error" : error.c_str()
            );
            return false;
        }

        if (driving_.camera() == Ecs::INVALID_ENTITY || !world_.alive(driving_.camera())) {
            std::fprintf(stderr, "[Driving]: no valid camera\n");
            return false;
        }

        return true;
    }

    void shutdown()
    {
        if (!started_) return;

        ::UI::shutdown();
        renderers_.shutdown();
        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        started_ = false;
    }

    void updateRendererKey()
    {
        const bool down = Keyboard.isKeyDown(Keyboard.KEY_RETURN);
        if (down && !renderer_key_down_ && !::UI::wantsKeyboard()) renderers_.next();
        renderer_key_down_ = down;
    }

    void updateDebugKey()
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

    Ecs::World world_;
    Scenes::Driving driving_;
    Renderer::Manager renderers_;
    Tests::RendererCheck renderer_check_{};
};

} // namespace Game

int main()
{
    Game::Application application;
    return application.run();
}
