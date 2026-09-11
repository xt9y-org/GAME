#include "Tests/RendererCheck.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

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

std::string_view rendererForTest(const char *test)
{
    if (!test) return "Rasterizer";
    const std::string_view name(test);
    if (name == "ray-tracer") return "Ray Tracer";
    if (name == "path-tracer") return "Path Tracer";
    return "Rasterizer";
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
        environment("RENDERCHECK_METRICS_PATH") != nullptr;
    if (!active_) return;

    metrics_path_ = environment("RENDERCHECK_METRICS_PATH");
    frame_limit_ = environmentUnsigned("RENDERCHECK_FRAME_LIMIT", 0u);
    renderer_name_ = rendererForTest(environment("RENDERCHECK_TEST"));
}

bool RendererCheck::lastFrame(std::uint64_t frame) const
{
    return active_ && frame_limit_ > 0u && frame + 1u >= frame_limit_;
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
