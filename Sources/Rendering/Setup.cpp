#include "Setup.hpp"

#include "../Debugging/Values.hpp"

#include <Renderer/GlobalIllumination/GlobalIllumination.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/PathTracer/PathTracer.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/RayTracer/RayTracer.hpp>

namespace Rendering {

void configure(Renderer::Manager& renderers)
{
    auto& rasterizer = renderers.add<Renderer::Rasterizer>("Rasterizer");
    rasterizer.setEnabled(true);
    rasterizer.setViewportCulling(true);
    rasterizer.setShadowResolution(Debugging::Values::ShadowResolutionDefault);
    rasterizer.setShadowCascades(Debugging::Values::ShadowCascadesDefault);
    rasterizer.setShadowDistance(Debugging::Values::ShadowDistanceDefault);
    rasterizer.setShadowNearPlane(Debugging::Values::ShadowNearDefault);
    rasterizer.setClearColor({0.0f, 0.0f, 0.0f, 0.0f});

    auto& ray_tracer = renderers.add<Renderer::RayTracer>("Ray Tracer");
    ray_tracer.setEnabled(true);
    ray_tracer.setResolutionDivisor(4);

    auto& path_tracer = renderers.add<Renderer::PathTracer>("Path Tracer");
    path_tracer.setEnabled(true);
    path_tracer.setResolutionDivisor(2);
    path_tracer.setSamplesPerFrame(2);
    path_tracer.setStationaryPhaseGrid(2);
    path_tracer.setResetPhaseGrid(1);
    path_tracer.setMovingPhaseGrid(4);
    path_tracer.setMovingDepthBlock(2);

    Renderer::GlobalIllumination::settings() =
        Renderer::GlobalIllumination::Settings{};
}

} // namespace Rendering
