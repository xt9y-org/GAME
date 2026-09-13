#include "Tests/Fixtures/RendererFixture.hpp"

#include "Input.hpp"
#include "Renderer/Systems/SceneCache.hpp"
#include "UI/UI.hpp"

#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>

namespace Testing {

bool RendererFixture::initialize(std::string_view renderer, std::string *error)
{
    if (error) error->clear();
    if (initialized_) return true;

    lwcglInstallFastRuntime();
#ifdef __APPLE__
    lwcglSetContextVersion(2, 1);
    lwcglSetContextProfile(LWCGL_CONTEXT_ANY_PROFILE);
#else
    lwcglSetContextVersion(4, 3);
    lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

    Display.setDisplayMode(new DisplayMode(width_, height_));
    Display.create();
    display_created_ = Display.isCreated() != LWCGL_FALSE;
    if (!display_created_) {
        if (error) *error = "display creation failed";
        return false;
    }
    Display.setTitle("Horse regression");

    Keyboard.create();
    keyboard_created_ = Keyboard.isCreated() != LWCGL_FALSE;
    Mouse.create();
    mouse_created_ = Mouse.isCreated() != LWCGL_FALSE;

    Renderer::Systems::SceneCache::setLeafSize(8u);
    Renderer::Systems::SceneCache::setMaximumTriangles(1000000u);

    manager_.setPostProcessPipeline(&post_process_);
    rasterizer_ = &manager_.add<Renderer::Rasterizer>("rasterizer");
    rasterizer_->setEnabled(true);
    rasterizer_->setViewportCulling(true);
    rasterizer_->setShadowResolution(1024);
    rasterizer_->setFallbackShadowResolution(512);
    rasterizer_->setMinimumShadowResolution(128);
    rasterizer_->setShadowNearPlane(0.05f);
    rasterizer_->setShadowFarScale(1.0f);
    rasterizer_->setDirectionalShadowDistance(80.0f);
    rasterizer_->setClearColor({0.025f, 0.03f, 0.04f, 1.0f});

    ray_tracer_ = &manager_.add<Renderer::RayTracer>("raytracer");
    ray_tracer_->setEnabled(true);
    ray_tracer_->setResolutionDivisor(2);

    path_tracer_ = &manager_.add<Renderer::PathTracer>("pathtracer");
    path_tracer_->setEnabled(true);
    path_tracer_->setResolutionDivisor(2);
    path_tracer_->setSamplesPerFrame(1);
    path_tracer_->setStationaryPhaseGrid(2);
    path_tracer_->setResetPhaseGrid(1);
    path_tracer_->setMovingPhaseGrid(4);
    path_tracer_->setMovingDepthBlock(4);

    if (!manager_.initialize()) {
        if (error) *error = "no renderer initialized";
        shutdown();
        return false;
    }
    if (!manager_.activate(renderer)) {
        if (error) *error = "requested renderer unavailable: " + std::string(renderer);
        shutdown();
        return false;
    }

    width_ = std::max(Display.getWidth(), 1);
    height_ = std::max(Display.getHeight(), 1);
    manager_.resize(width_, height_);
    initialized_ = true;
    return true;
}

void RendererFixture::processEvents()
{
    if (!display_created_) return;
    Display.processMessages();
    Input::poll();
    const int next_width = std::max(Display.getWidth(), 1);
    const int next_height = std::max(Display.getHeight(), 1);
    if (next_width != width_ || next_height != height_) resize(next_width, next_height);
}

void RendererFixture::render(const Ecs::World& world)
{
    manager_.render(world);
}

void RendererFixture::resize(int width, int height)
{
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    manager_.resize(width_, height_);
}

void RendererFixture::shutdown()
{
    UI::shutdown();
    if (initialized_ || manager_.count() > 0u) manager_.shutdown();
    initialized_ = false;
    if (mouse_created_) Mouse.destroy();
    mouse_created_ = false;
    if (keyboard_created_) Keyboard.destroy();
    keyboard_created_ = false;
    if (display_created_) Display.destroy();
    display_created_ = false;
}

} // namespace Testing
