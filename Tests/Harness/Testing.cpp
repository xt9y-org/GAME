#include "Tests/Harness/Testing.hpp"

#include "Animation/Animation.hpp"
#include "Audio/Audio.hpp"
#include "Input.hpp"
#include "Models/Models.hpp"
#include "Renderer/Debug/Debug.hpp"
#include "Tests/Fixtures/RendererFixture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Testing {
namespace {

using Clock = std::chrono::steady_clock;

const char *environment(const char *name)
{
    const char *value = std::getenv(name);
    return value && *value ? value : nullptr;
}

std::size_t rendererCheckFrameLimit(std::size_t fallback)
{
    if (const char *value = environment("RENDERCHECK_FRAME_LIMIT")) {
        char *end = nullptr;
        const unsigned long long parsed = std::strtoull(value, &end, 10);
        if (end && *end == '\0' && parsed > 0u)
            return static_cast<std::size_t>(parsed);
    }
    return environment("RENDERCHECK") ? std::max<std::size_t>(fallback, 120u) : fallback;
}

void metric(const char *name, double value)
{
    const char *path = environment("RENDERCHECK_METRICS_PATH");
    if (!path || !std::isfinite(value)) return;
    FILE *file = std::fopen(path, "a");
    if (!file) return;
    std::fprintf(file, "%s=%.6f\n", name, value);
    std::fclose(file);
}

void cleanup(Context& context)
{
    if (context.graphics) {
        context.graphics->shutdown();
        context.graphics.reset();
    }
    Renderer::Debug::shutdown();
    Input::reset();
    Audio::clearClips();
    Animation::clearAssets();
    Models::clearCache();
}

} // namespace

Context::Context() = default;
Context::~Context() = default;

void Runner::add(std::unique_ptr<Case> test)
{
    if (test) cases_.push_back(std::move(test));
}

int Runner::run(int argc, char **argv)
{
    std::string selected;
    std::string renderer = "rasterizer";
    bool renderer_explicit = false;
    bool list = false;

    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (std::strcmp(argument, "--list") == 0) {
            list = true;
        } else if (std::strcmp(argument, "--test") == 0 && index + 1 < argc) {
            selected = argv[++index];
        } else if (std::strcmp(argument, "--renderer") == 0 && index + 1 < argc) {
            renderer = argv[++index];
            renderer_explicit = true;
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argument);
            return 2;
        }
    }

    if (list) {
        for (const auto& test : cases_)
            std::printf("%.*s\n", static_cast<int>(test->name().size()), test->name().data());
        return 0;
    }

    if (selected.empty()) {
        if (const char *value = environment("RENDERCHECK_TEST")) selected = value;
        else {
            std::fprintf(stderr, "Select a regression case with --test <name> (use --list to enumerate).\n");
            return 2;
        }
    }

    if (!renderer_explicit) {
        if (selected == "rendering/raytracer") renderer = "raytracer";
        else if (selected == "rendering/pathtracer") renderer = "pathtracer";
        else if (selected == "rendering/rasterizer") renderer = "rasterizer";
    }

    Case *test = nullptr;
    for (const auto& candidate : cases_) {
        if (candidate->name() == selected) {
            test = candidate.get();
            break;
        }
    }
    if (!test) {
        std::fprintf(stderr, "Unknown regression case: %s\n", selected.c_str());
        return 2;
    }

    Context context;
    context.renderer = renderer;
    std::string error;

    if (test->kind() == Kind::Visual) {
        context.graphics = std::make_unique<RendererFixture>();
        if (!context.graphics->initialize(renderer, &error)) {
            std::fprintf(stderr, "[%s] graphics setup failed: %s\n", selected.c_str(), error.c_str());
            cleanup(context);
            return 1;
        }
    }

    if (!test->setup(context, error)) {
        std::fprintf(stderr, "[%s] setup failed: %s\n", selected.c_str(), error.c_str());
        test->shutdown(context);
        cleanup(context);
        return 1;
    }

    const std::size_t frame_count = rendererCheckFrameLimit(test->frameCount());
    for (std::size_t frame = 0u; frame < frame_count; ++frame) {
        const auto started = Clock::now();
        if (context.graphics) context.graphics->processEvents();
        if (!test->update(context, 1.0 / 60.0, error)) {
            std::fprintf(stderr, "[%s] update failed: %s\n", selected.c_str(), error.c_str());
            test->shutdown(context);
            cleanup(context);
            return 1;
        }
        if (context.graphics) context.graphics->render(context.world);
        const double frame_ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        metric("frame_ms", frame_ms);
    }

    const bool passed = test->verify(context, error);
    test->shutdown(context);
    cleanup(context);
    if (!passed) {
        std::fprintf(stderr, "[%s] verification failed: %s\n", selected.c_str(), error.c_str());
        return 1;
    }

    std::printf("PASS %s\n", selected.c_str());
    return 0;
}

bool near(float a, float b, float epsilon)
{
    return std::abs(a - b) <= epsilon;
}

bool near(double a, double b, double epsilon)
{
    return std::abs(a - b) <= epsilon;
}

bool require(bool condition, std::string_view message, std::string& error)
{
    if (condition) return true;
    error.assign(message.data(), message.size());
    return false;
}

} // namespace Testing
