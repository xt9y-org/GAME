#include "Tests/Fixtures/RendererFixture.hpp"

#include "Input/Input.hpp"
#include "Renderer/SDLGPU/Context.hpp"
#include "Renderer/Systems/SceneCache.hpp"
#include "UI/UI.hpp"
#include "Window/Window.hpp"

#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace {

const char *environment(const char *name)
{
    const char *value = std::getenv(name);
    return value && *value ? value : nullptr;
}

std::uint64_t captureFrameIndex()
{
    const char *value = environment("RENDERCHECK_CAPTURE_FRAME");
    if (!value) return 0u;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    return end && *end == '\0' ? static_cast<std::uint64_t>(parsed) : 0u;
}

bool captureDue(std::uint64_t frame)
{
    return environment("RENDERCHECK_CAPTURE_PATH") && frame == captureFrameIndex();
}

float halfToFloat(std::uint16_t value)
{
    const bool negative = (value & 0x8000u) != 0u;
    const unsigned exponent = (value >> 10u) & 0x1fu;
    const unsigned mantissa = value & 0x03ffu;

    float result = 0.0f;
    if (exponent == 0u) {
        result = mantissa == 0u ? 0.0f : std::ldexp(static_cast<float>(mantissa), -24);
    } else if (exponent == 31u) {
        result = mantissa == 0u
            ? std::numeric_limits<float>::infinity()
            : std::numeric_limits<float>::quiet_NaN();
    } else {
        result = std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
                            static_cast<int>(exponent) - 15);
    }
    return negative ? -result : result;
}

bool srgbSwapchain()
{
    const SDL_GPUTextureFormat format = Renderer::SDLGPU::swapchainFormat();
    return format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB ||
        format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
}

std::uint8_t channelByte(float value, bool srgb)
{
    if (!std::isfinite(value)) value = value > 0.0f ? 1.0f : 0.0f;
    value = std::clamp(value, 0.0f, 1.0f);
    if (srgb) {
        value = value <= 0.0031308f
            ? value * 12.92f
            : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    }
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

bool writeCapture(void *texture, int width, int height, std::string *error)
{
    const char *path = environment("RENDERCHECK_CAPTURE_PATH");
    SDL_GPUDevice *device = Renderer::SDLGPU::device();
    if (!path || !texture || !device || width <= 0 || height <= 0) {
        if (error) *error = "RendererCheck capture target is unavailable";
        return false;
    }

    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixels > std::numeric_limits<Uint32>::max() / 8u) {
        if (error) *error = "RendererCheck capture target is too large";
        return false;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = static_cast<Uint32>(pixels * 8u);
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) {
        if (error) *error = std::string("RendererCheck transfer buffer creation failed: ") + SDL_GetError();
        return false;
    }

    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy = command ? SDL_BeginGPUCopyPass(command) : nullptr;
    if (!copy) {
        if (command) SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (error) *error = std::string("RendererCheck copy pass creation failed: ") + SDL_GetError();
        return false;
    }

    SDL_GPUTextureRegion source{};
    source.texture = static_cast<SDL_GPUTexture *>(texture);
    source.w = static_cast<Uint32>(width);
    source.h = static_cast<Uint32>(height);
    source.d = 1u;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = transfer;
    destination.pixels_per_row = static_cast<Uint32>(width);
    destination.rows_per_layer = static_cast<Uint32>(height);
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
    if (!fence) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (error) *error = std::string("RendererCheck capture submit failed: ") + SDL_GetError();
        return false;
    }
    SDL_GPUFence *fences[] = {fence};
    if (!SDL_WaitForGPUFences(device, true, fences, 1u)) {
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (error) *error = std::string("RendererCheck capture wait failed: ") + SDL_GetError();
        return false;
    }

    const auto *values = static_cast<const std::uint16_t *>(
        SDL_MapGPUTransferBuffer(device, transfer, false));
    if (!values) {
        SDL_ReleaseGPUFence(device, fence);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (error) *error = std::string("RendererCheck capture map failed: ") + SDL_GetError();
        return false;
    }

    FILE *file = std::fopen(path, "wb");
    bool ok = file && std::fprintf(file, "P6\n%d %d\n255\n", width, height) >= 0;
    std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3u);
    const bool srgb = srgbSwapchain();
    for (int y = 0; ok && y < height; ++y) {
        const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u;
        for (int x = 0; x < width; ++x) {
            const std::size_t source_offset = row_offset + static_cast<std::size_t>(x) * 4u;
            const std::size_t destination_offset = static_cast<std::size_t>(x) * 3u;
            row[destination_offset + 0u] = channelByte(halfToFloat(values[source_offset + 0u]), srgb);
            row[destination_offset + 1u] = channelByte(halfToFloat(values[source_offset + 1u]), srgb);
            row[destination_offset + 2u] = channelByte(halfToFloat(values[source_offset + 2u]), srgb);
        }
        ok = std::fwrite(row.data(), 1u, row.size(), file) == row.size();
    }
    if (file && std::fclose(file) != 0) ok = false;

    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUFence(device, fence);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (!ok && error) *error = "RendererCheck capture file write failed";
    return ok;
}

class CaptureProbePass final : public Renderer::PostProcess::Pass {
public:
    CaptureProbePass(void **texture, int *width, int *height)
        : texture_(texture), width_(width), height_(height) {}

    bool process(Renderer::PostProcess::Frame& frame) override
    {
        *texture_ = frame.color_texture;
        *width_ = frame.width;
        *height_ = frame.height;
        return true;
    }

private:
    void **texture_;
    int *width_;
    int *height_;
};

} // namespace

namespace Testing {

bool RendererFixture::initialize(std::string_view renderer, std::string *error)
{
    if (error) error->clear();
    if (initialized_) return true;

    window_created_ = Window::create(Window::Settings{
        .title = "Horse regression",
        .width = width_,
        .height = height_,
    });
    if (!window_created_) {
        if (error) *error = "window creation failed";
        return false;
    }
    Input::reset();

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

    width_ = std::max(Window::width(), 1);
    height_ = std::max(Window::height(), 1);
    manager_.resize(width_, height_);
    initialized_ = true;
    return true;
}

void RendererFixture::processEvents()
{
    if (!window_created_) return;
    Window::poll();
    Input::poll();
    const int next_width = std::max(Window::width(), 1);
    const int next_height = std::max(Window::height(), 1);
    if (next_width != width_ || next_height != height_) resize(next_width, next_height);
}

bool RendererFixture::render(const Ecs::World& world, std::string *error)
{
    if (error) error->clear();
    if (!capture_probe_added_ && environment("RENDERCHECK_CAPTURE_PATH")) {
        post_process_.add<CaptureProbePass>(&capture_texture_, &capture_width_, &capture_height_);
        capture_probe_added_ = true;
    }
    manager_.render(world);
    const bool ok = !captureDue(frame_index_) ||
        writeCapture(capture_texture_, capture_width_, capture_height_, error);
    ++frame_index_;
    return ok;
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
    capture_probe_added_ = false;
    frame_index_ = 0u;
    capture_texture_ = nullptr;
    capture_width_ = 0;
    capture_height_ = 0;
    Input::reset();
    if (window_created_) Window::destroy();
    window_created_ = false;
}

} // namespace Testing
