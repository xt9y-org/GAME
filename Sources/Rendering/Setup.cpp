#include "Setup.hpp"

#include "../Debugging/Values.hpp"

#include <Renderer/GlobalIllumination/GlobalIllumination.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Volumetrics/Volumetrics.hpp>

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

    Renderer::GlobalIllumination::settings() =
        Renderer::GlobalIllumination::Settings{};

    Renderer::Volumetrics::settings() = Renderer::Volumetrics::Settings{};
    Renderer::Volumetrics::settings().enabled = false;
}

} // namespace Rendering
