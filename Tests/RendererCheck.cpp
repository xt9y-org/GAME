#include "Tests/RendererCheck.hpp"

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
