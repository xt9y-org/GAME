#include "Tests/RendererCheck.hpp"

#include "Sources/Camera.hpp"
#include "Sources/Renderer/Components.hpp"
#include "Sources/Renderer/PathTracer/PathTracer.hpp"
#include "Sources/Renderer/Rasterizer/Rasterizer.hpp"

#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace Game::Tests {
namespace {

const char *environment(const char *name)
{
    const char *value = std::getenv(name);
    return value && *value ? value : nullptr;
}

std::uint64_t environmentUnsigned(const char *name, std::uint64_t fallback)
{
    const char *value = environment(name);
    if (!value || *value == '-') return fallback;

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno != 0 || !end || *end != '\0') return fallback;
    return static_cast<std::uint64_t>(parsed);
}

bool metricNameValid(std::string_view name)
{
    if (name.empty()) return false;
    for (const unsigned char c : name) {
        const bool alpha_numeric =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9');
        if (!alpha_numeric && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

} // namespace

RendererCheck::RendererCheck()
{
    active_ = environment("RENDERCHECK") != nullptr ||
        environment("RENDERCHECK_CAPTURE_PATH") != nullptr ||
        environment("RENDERCHECK_METRICS_PATH") != nullptr;
    if (!active_) return;

    if (const char *value = environment("RENDERCHECK_TEST")) test_ = value;
    if (const char *value = environment("RENDERCHECK_PERF_CASE")) performance_case_ = value;
    capture_path_ = environment("RENDERCHECK_CAPTURE_PATH");
    metrics_path_ = environment("RENDERCHECK_METRICS_PATH");
    capture_frame_ = environmentUnsigned("RENDERCHECK_CAPTURE_FRAME", 0u);
    frame_limit_ = environmentUnsigned("RENDERCHECK_FRAME_LIMIT", 0u);
}

std::string_view RendererCheck::profile() const
{
    return visual() ? test_ : performance_case_;
}

std::string_view RendererCheck::rendererName() const
{
    const std::string_view value = profile();
    if (value == "path-stationary" || value == "path-turn") return "Path Tracer";
    return "Rasterizer";
}

bool RendererCheck::captureDue(std::uint64_t frame) const
{
    return active_ && capture_path_ && frame == capture_frame_;
}

bool RendererCheck::lastFrame(std::uint64_t frame) const
{
    return active_ && frame_limit_ > 0u && frame + 1u >= frame_limit_;
}

bool RendererCheck::captureOpenGL(int width, int height) const
{
    if (!capture_path_ || width <= 0 || height <= 0) return false;

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3u;
    if (row_bytes > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(height))
        return false;

    std::vector<unsigned char> pixels(row_bytes * static_cast<std::size_t>(height));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    FILE *file = std::fopen(capture_path_, "wb");
    if (!file) return false;
    if (std::fprintf(file, "P6\n%d %d\n255\n", width, height) < 0) {
        std::fclose(file);
        return false;
    }

    bool ok = true;
    for (int y = height - 1; y >= 0; --y) {
        const unsigned char *row = pixels.data() + static_cast<std::size_t>(y) * row_bytes;
        if (std::fwrite(row, 1u, row_bytes, file) != row_bytes) {
            ok = false;
            break;
        }
    }
    if (std::fclose(file) != 0) ok = false;
    return ok;
}

void RendererCheck::configure(Renderer::Rasterizer& rasterizer) const
{
    const std::string_view value = profile();
    if (value == "horizon-gi") {
        rasterizer.setLightingResolutionDivisor(1);
        rasterizer.setShadowResolutionDivisor(1);
        rasterizer.setDepthAwareUpscaling(true);
        rasterizer.setTemporalUpscaling(false);
        rasterizer.setHorizonGiEnabled(true);
        rasterizer.setHorizonGiResolutionDivisor(2);
        rasterizer.setHorizonGiDirections(4);
        rasterizer.setHorizonGiSteps(6);
        rasterizer.setHorizonGiTemporalFilter(false);
        return;
    }

    if (value == "low-end") {
        rasterizer.setLightingResolutionDivisor(2);
        rasterizer.setShadowResolutionDivisor(4);
        rasterizer.setDepthAwareUpscaling(true);
        rasterizer.setTemporalUpscaling(false);
        rasterizer.setHorizonGiEnabled(true);
        rasterizer.setHorizonGiResolutionDivisor(2);
        rasterizer.setHorizonGiDirections(2);
        rasterizer.setHorizonGiSteps(4);
        rasterizer.setHorizonGiTemporalFilter(false);
    }
}

void RendererCheck::configure(Renderer::PathTracer& path_tracer) const
{
    const std::string_view value = profile();
    if (value != "path-stationary" && value != "path-turn") return;

    path_tracer.setResolutionDivisor(2);
    path_tracer.setSamplesPerFrame(1);
    path_tracer.setStationaryPhaseGrid(2);
    path_tracer.setResetPhaseGrid(1);
    path_tracer.setMovingPhaseGrid(4);
    path_tracer.setMovingDepthBlock(4);
}

void RendererCheck::update(Ecs::World& world) const
{
    if (profile() != "path-turn") return;
    const Ecs::Entity camera = Camera::activeCamera(world);
    if (camera == Ecs::INVALID_ENTITY) return;
    Renderer::Transform *transform = world.get<Renderer::Transform>(camera);
    if (transform) transform->rotation.y += 1.0f;
}

void RendererCheck::record(const Renderer::Rasterizer& rasterizer) const
{
    const Renderer::RasterizerStatistics raster = rasterizer.statistics();
    const Renderer::HorizonGI::Statistics horizon = rasterizer.horizonGiStatistics();
    const Renderer::Upscale::Statistics upscale = rasterizer.upscaleStatistics();

    metric("rasterizer_scaled_pipeline", raster.scaled_pipeline_active ? 1.0 : 0.0);
    metric("rasterizer_output_width", static_cast<double>(raster.output_width));
    metric("rasterizer_output_height", static_cast<double>(raster.output_height));
    metric("rasterizer_lighting_width", static_cast<double>(raster.lighting_width));
    metric("rasterizer_lighting_height", static_cast<double>(raster.lighting_height));
    metric("rasterizer_shadow_active", raster.shadow_active ? 1.0 : 0.0);
    metric("rasterizer_shadow_resolution", static_cast<double>(raster.shadow_resolution));
    metric("rasterizer_depth_prepass_items", static_cast<double>(raster.depth_prepass_items));

    metric("horizon_gi_active", horizon.active ? 1.0 : 0.0);
    metric("horizon_gi_indirect", horizon.indirect ? 1.0 : 0.0);
    metric("horizon_gi_history", horizon.temporal_history ? 1.0 : 0.0);
    metric("horizon_gi_width", static_cast<double>(horizon.width));
    metric("horizon_gi_height", static_cast<double>(horizon.height));
    metric("horizon_gi_directions", static_cast<double>(horizon.directions));
    metric("horizon_gi_steps", static_cast<double>(horizon.steps));

    metric("upscale_active", upscale.active ? 1.0 : 0.0);
    metric("upscale_depth_aware", upscale.depth_aware ? 1.0 : 0.0);
    metric("upscale_effect", upscale.effect ? 1.0 : 0.0);
    metric("upscale_history", upscale.temporal_history ? 1.0 : 0.0);
    metric("upscale_source_width", static_cast<double>(upscale.source_width));
    metric("upscale_source_height", static_cast<double>(upscale.source_height));
    metric("upscale_output_width", static_cast<double>(upscale.output_width));
    metric("upscale_output_height", static_cast<double>(upscale.output_height));
}

void RendererCheck::record(const Renderer::PathTracer& path_tracer) const
{
    const Renderer::PathTracerStatistics path = path_tracer.statistics();
    metric("path_active", path.active ? 1.0 : 0.0);
    metric("path_output_width", static_cast<double>(path.output_width));
    metric("path_output_height", static_cast<double>(path.output_height));
    metric("path_trace_width", static_cast<double>(path.trace_width));
    metric("path_trace_height", static_cast<double>(path.trace_height));
    metric("path_samples_per_frame", static_cast<double>(path.samples_per_frame));
    metric("path_stationary_phase_grid", static_cast<double>(path.stationary_phase_grid));
    metric("path_reset_phase_grid", static_cast<double>(path.reset_phase_grid));
    metric("path_moving_phase_grid", static_cast<double>(path.moving_phase_grid));
    metric("path_moving_depth_block", static_cast<double>(path.moving_depth_block));
    metric("path_stationary_pixel_budget", static_cast<double>(path.stationary_path_pixel_budget));
    metric("path_reset_pixel_budget", static_cast<double>(path.reset_path_pixel_budget));
    metric("path_moving_pixel_budget", static_cast<double>(path.moving_path_pixel_budget));
    metric("path_moving_depth_ray_budget", static_cast<double>(path.moving_depth_ray_budget));
}

void RendererCheck::metric(std::string_view name, double value) const
{
    if (!metrics_path_ || !metricNameValid(name) || !std::isfinite(value) || value < 0.0) return;

    FILE *file = std::fopen(metrics_path_, "a");
    if (!file) return;
    const std::string metric_name(name);
    std::fprintf(file, "%s=%.9f\n", metric_name.c_str(), value);
    std::fclose(file);
}

} // namespace Game::Tests
