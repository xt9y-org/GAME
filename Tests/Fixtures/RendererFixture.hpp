#ifndef GAME_TESTS_FIXTURES_RENDERER_FIXTURE_HPP
#define GAME_TESTS_FIXTURES_RENDERER_FIXTURE_HPP

#include "Camera.hpp"
#include "Renderer/Manager.hpp"
#include "Renderer/PathTracer/PathTracer.hpp"
#include "Renderer/PostProcess.hpp"
#include "Renderer/Rasterizer/Rasterizer.hpp"
#include "Renderer/RayTracer/RayTracer.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Testing {

class RendererFixture {
public:
    bool initialize(std::string_view renderer, std::string *error = nullptr);
    void processEvents();
    bool render(const Ecs::World& world, std::string *error = nullptr);
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
    bool window_created_ = false;
    bool initialized_ = false;
    bool capture_probe_added_ = false;
    std::uint64_t frame_index_ = 0u;
    void *capture_texture_ = nullptr;
    int capture_width_ = 0;
    int capture_height_ = 0;
};

} // namespace Testing

#endif
