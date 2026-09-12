#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Renderer/PathTracer/PathTracer.hpp"
#include "Renderer/Rasterizer/Rasterizer.hpp"
#include "Renderer/RayTracer/RayTracer.hpp"

namespace Tests {
namespace {

class RasterizerEnabledCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/enabled"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setEnabled(true);
        return Testing::require(renderer.enabled(), "rasterizer enabled did not round-trip", error);
    }
};

class RasterizerViewportCullingCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/viewport-culling"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setViewportCulling(true);
        return Testing::require(renderer.viewportCulling(), "rasterizer viewport culling did not round-trip", error);
    }
};

class RasterizerShadowResolutionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/shadow-resolution"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setShadowResolution(2048);
        return Testing::require(renderer.shadowResolution() == 2048, "rasterizer shadow resolution mismatch", error);
    }
};

class RasterizerFallbackShadowResolutionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/fallback-shadow-resolution"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setFallbackShadowResolution(768);
        return Testing::require(renderer.fallbackShadowResolution() == 768, "rasterizer fallback shadow resolution mismatch", error);
    }
};

class RasterizerMinimumShadowResolutionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/minimum-shadow-resolution"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setMinimumShadowResolution(192);
        return Testing::require(renderer.minimumShadowResolution() == 192, "rasterizer minimum shadow resolution mismatch", error);
    }
};

class RasterizerShadowNearPlaneCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/shadow-near-plane"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setShadowNearPlane(0.035f);
        return Testing::require(Testing::near(renderer.shadowNearPlane(), 0.035f), "rasterizer shadow near plane mismatch", error);
    }
};

class RasterizerShadowFarScaleCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/shadow-far-scale"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setShadowFarScale(1.75f);
        return Testing::require(Testing::near(renderer.shadowFarScale(), 1.75f), "rasterizer shadow far scale mismatch", error);
    }
};

class RasterizerDirectionalShadowDistanceCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/directional-shadow-distance"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setDirectionalShadowDistance(96.0f);
        return Testing::require(Testing::near(renderer.directionalShadowDistance(), 96.0f), "rasterizer directional shadow distance mismatch", error);
    }
};

class RasterizerClearColorCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/rasterizer/clear-color"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer renderer;
        renderer.setClearColor({0.1f, 0.2f, 0.3f, 0.4f});
        const Renderer::Vec4 color = renderer.clearColor();
        return Testing::require(
            Testing::near(color.x, 0.1f) && Testing::near(color.y, 0.2f) &&
            Testing::near(color.z, 0.3f) && Testing::near(color.w, 0.4f),
            "rasterizer clear color mismatch", error);
    }
};

class RayTracerEnabledCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/raytracer/enabled"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::RayTracer renderer;
        renderer.setEnabled(true);
        return Testing::require(renderer.enabled(), "ray tracer enabled did not round-trip", error);
    }
};

class RayTracerResolutionDivisorCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/raytracer/resolution-divisor"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::RayTracer renderer;
        renderer.setResolutionDivisor(3);
        return Testing::require(renderer.resolutionDivisor() == 3, "ray tracer resolution divisor mismatch", error);
    }
};

class PathTracerEnabledCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/enabled"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setEnabled(true);
        return Testing::require(renderer.enabled(), "path tracer enabled did not round-trip", error);
    }
};

class PathTracerResolutionDivisorCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/resolution-divisor"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setResolutionDivisor(3);
        return Testing::require(renderer.resolutionDivisor() == 3, "path tracer resolution divisor mismatch", error);
    }
};

class PathTracerSamplesPerFrameCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/samples-per-frame"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setSamplesPerFrame(5);
        return Testing::require(renderer.samplesPerFrame() == 5, "path tracer samples per frame mismatch", error);
    }
};

class PathTracerStationaryPhaseGridCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/stationary-phase-grid"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setStationaryPhaseGrid(3);
        return Testing::require(renderer.stationaryPhaseGrid() == 3, "path tracer stationary phase grid mismatch", error);
    }
};

class PathTracerResetPhaseGridCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/reset-phase-grid"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setResetPhaseGrid(2);
        return Testing::require(renderer.resetPhaseGrid() == 2, "path tracer reset phase grid mismatch", error);
    }
};

class PathTracerMovingPhaseGridCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/moving-phase-grid"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setMovingPhaseGrid(5);
        return Testing::require(renderer.movingPhaseGrid() == 5, "path tracer moving phase grid mismatch", error);
    }
};

class PathTracerMovingDepthBlockCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings/pathtracer/moving-depth-block"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PathTracer renderer;
        renderer.setMovingDepthBlock(9);
        return Testing::require(renderer.movingDepthBlock() == 9, "path tracer moving depth block mismatch", error);
    }
};

} // namespace

void registerPublicRendererSettings(Testing::Runner& runner)
{
    runner.add<RasterizerEnabledCase>();
    runner.add<RasterizerViewportCullingCase>();
    runner.add<RasterizerShadowResolutionCase>();
    runner.add<RasterizerFallbackShadowResolutionCase>();
    runner.add<RasterizerMinimumShadowResolutionCase>();
    runner.add<RasterizerShadowNearPlaneCase>();
    runner.add<RasterizerShadowFarScaleCase>();
    runner.add<RasterizerDirectionalShadowDistanceCase>();
    runner.add<RasterizerClearColorCase>();
    runner.add<RayTracerEnabledCase>();
    runner.add<RayTracerResolutionDivisorCase>();
    runner.add<PathTracerEnabledCase>();
    runner.add<PathTracerResolutionDivisorCase>();
    runner.add<PathTracerSamplesPerFrameCase>();
    runner.add<PathTracerStationaryPhaseGridCase>();
    runner.add<PathTracerResetPhaseGridCase>();
    runner.add<PathTracerMovingPhaseGridCase>();
    runner.add<PathTracerMovingDepthBlockCase>();
}

} // namespace Tests
