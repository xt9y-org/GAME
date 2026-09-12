#ifndef GAME_TESTS_FIXTURES_RENDERER_FIXTURE_HPP
#define GAME_TESTS_FIXTURES_RENDERER_FIXTURE_HPP

#include "Renderer/Manager.hpp"
#include "Renderer/PathTracer/PathTracer.hpp"
#include "Renderer/PostProcess.hpp"
#include "Renderer/Rasterizer/Rasterizer.hpp"
#include "Renderer/RayTracer/RayTracer.hpp"

#include <string>
#include <string_view>

namespace Testing {

class RendererFixture {
public:
    bool initialize(std::string_view renderer, std::string *error = nullptr);
    void processEvents();
    void render(const Ecs::World& world);
    void resize(int width, int height);
    void shutdown();

    Renderer::Manager& manager() { return manager_; }
    Renderer::Rasterizer& rasterizer() { return *rasterizer_; }
    Renderer::RayTracer& rayTracer() { return *ray_tracer_; }
    Renderer::PathTracer& pathTracer() { return *path_tracer_; }
    Renderer::PostProcess::Pipeline& postProcess() { return post_process_; }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    Renderer::Manager manager_;
    Renderer::PostProcess::Pipeline post_process_;
    Renderer::Rasterizer *rasterizer_ = nullptr;
    Renderer::RayTracer *ray_tracer_ = nullptr;
    Renderer::PathTracer *path_tracer_ = nullptr;
    int width_ = 640;
    int height_ = 360;
    bool display_created_ = false;
    bool keyboard_created_ = false;
    bool mouse_created_ = false;
    bool initialized_ = false;
};

} // namespace Testing

#endif
