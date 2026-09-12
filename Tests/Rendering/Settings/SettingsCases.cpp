#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Tests/Fixtures/RendererFixture.hpp"
#include "Tests/Fixtures/SceneFixture.hpp"

#include "Renderer/PathTracer/PathTracer.hpp"
#include "Renderer/PostProcess.hpp"
#include "Renderer/Rasterizer/Rasterizer.hpp"
#include "Renderer/RayTracer/RayTracer.hpp"

#include <string>
#include <utility>

namespace Tests {
namespace {

class CountingPass final : public Renderer::PostProcess::Pass {
public:
    bool process(Renderer::PostProcess::Frame& frame) override
    {
        ++processed;
        observed_width = frame.width;
        return succeed;
    }

    void shutdown() override { ++shutdowns; }

    bool succeed = true;
    int processed = 0;
    int shutdowns = 0;
    int observed_width = 0;
};

class RendererSettingsCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/settings"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::Rasterizer rasterizer;
        rasterizer.setEnabled(true);
        rasterizer.setViewportCulling(true);
        rasterizer.setShadowResolution(2048);
        rasterizer.setFallbackShadowResolution(512);
        rasterizer.setMinimumShadowResolution(128);
        rasterizer.setShadowNearPlane(0.03f);
        rasterizer.setShadowFarScale(1.5f);
        rasterizer.setDirectionalShadowDistance(120.0f);
        rasterizer.setClearColor({0.1f, 0.2f, 0.3f, 1.0f});

        Renderer::RayTracer ray;
        ray.setEnabled(true);
        ray.setResolutionDivisor(3);

        Renderer::PathTracer path;
        path.setEnabled(true);
        path.setResolutionDivisor(2);
        path.setSamplesPerFrame(4);
        path.setStationaryPhaseGrid(2);
        path.setResetPhaseGrid(1);
        path.setMovingPhaseGrid(4);
        path.setMovingDepthBlock(8);

        return Testing::require(rasterizer.enabled() && rasterizer.viewportCulling(), "rasterizer flags mismatch", error) &&
            Testing::require(rasterizer.shadowResolution() == 2048 && rasterizer.fallbackShadowResolution() == 512 &&
                             rasterizer.minimumShadowResolution() == 128, "rasterizer shadow settings mismatch", error) &&
            Testing::require(Testing::near(rasterizer.shadowNearPlane(), 0.03f) &&
                             Testing::near(rasterizer.shadowFarScale(), 1.5f) &&
                             Testing::near(rasterizer.directionalShadowDistance(), 120.0f),
                             "rasterizer distance settings mismatch", error) &&
            Testing::require(ray.enabled() && ray.resolutionDivisor() == 3, "ray tracer settings mismatch", error) &&
            Testing::require(path.enabled() && path.resolutionDivisor() == 2 && path.samplesPerFrame() == 4 &&
                             path.stationaryPhaseGrid() == 2 && path.resetPhaseGrid() == 1 &&
                             path.movingPhaseGrid() == 4 && path.movingDepthBlock() == 8,
                             "path tracer settings mismatch", error);
    }
};

class PostProcessCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/post-process"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PostProcess::Pipeline pipeline;
        CountingPass& first = pipeline.add<CountingPass>();
        CountingPass& second = pipeline.add<CountingPass>();
        Renderer::PostProcess::Frame frame;
        frame.width = 320;
        if (!Testing::require(pipeline.count() == 2u, "post-process pass count mismatch", error)) return false;
        if (!Testing::require(pipeline.process(frame), "post-process pipeline rejected frame", error)) return false;
        if (!Testing::require(first.processed == 1 && second.processed == 1 && first.observed_width == 320,
                              "post-process passes not invoked", error)) return false;
        pipeline.shutdown();
        return Testing::require(first.shutdowns == 1 && second.shutdowns == 1, "post-process shutdown mismatch", error);
    }
};

class FailedPostProcessCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/post-process-failure"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Renderer::PostProcess::Pipeline pipeline;
        CountingPass& first = pipeline.add<CountingPass>();
        CountingPass& second = pipeline.add<CountingPass>();
        first.succeed = false;
        Renderer::PostProcess::Frame frame;
        const bool result = pipeline.process(frame);
        return Testing::require(!result, "failing post-process pass was ignored", error) &&
            Testing::require(first.processed == 1 && second.processed == 0,
                             "post-process pipeline did not short-circuit", error);
    }
};

class RenderSmokeCase final : public Testing::Case {
public:
    explicit RenderSmokeCase(std::string name) : name_(std::move(name)) {}

    std::string_view name() const override { return name_; }
    Testing::Kind kind() const override { return Testing::Kind::Visual; }
    std::size_t frameCount() const override { return 4u; }

    bool setup(Testing::Context& context, std::string&) override
    {
        Models::clearCache();
        Testing::addCamera(context.world);
        const Testing::SceneAssets assets = Testing::triangleAssets();
        Testing::addTriangle(context.world, assets);
        Testing::addLighting(context.world);
        return true;
    }

    bool verify(Testing::Context& context, std::string& error) override
    {
        return Testing::require(context.graphics && context.graphics->manager().active() != nullptr,
                                "renderer smoke test has no active renderer", error);
    }

private:
    std::string name_;
};

} // namespace

void registerSettings(Testing::Runner& runner)
{
    runner.add<RendererSettingsCase>();
    runner.add<PostProcessCase>();
    runner.add<FailedPostProcessCase>();
    runner.add<RenderSmokeCase>("rendering/rasterizer");
    runner.add<RenderSmokeCase>("rendering/raytracer");
    runner.add<RenderSmokeCase>("rendering/pathtracer");
}

} // namespace Tests
