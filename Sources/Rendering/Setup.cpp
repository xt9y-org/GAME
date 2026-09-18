#include "Setup.hpp"

#include "../Debugging/Values.hpp"

#include <Renderer/Features.hpp>
#include <Renderer/GaussianSplat/GaussianSplat.hpp>
#include <Renderer/GlobalIllumination/GlobalIllumination.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/Quality.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Volumetrics/Volumetrics.hpp>

namespace Rendering {

void configure(Renderer::Manager& renderers)
{
    auto& rasterizer = renderers.add<Renderer::Rasterizer>("Rasterizer");
    rasterizer.setEnabled(true);
    rasterizer.setViewportCulling(true);
    rasterizer.setOcclusionCulling(true);
    rasterizer.setShadowQuality(Renderer::Quality::High);
    rasterizer.setShadowDistance(Debugging::Values::ShadowDistanceDefault);
    rasterizer.setShadowNearPlane(Debugging::Values::ShadowNearDefault);
    rasterizer.setClearColor({0.0f, 0.0f, 0.0f, 0.0f});

    Renderer::GlobalIllumination::settings() =
        Renderer::GlobalIllumination::Settings{};
    Renderer::GlobalIllumination::setQuality(Renderer::Quality::High);

    Renderer::Volumetrics::settings() = Renderer::Volumetrics::Settings{};
    Renderer::Volumetrics::setQuality(Renderer::Quality::High);

    Renderer::GaussianSplat::settings() = Renderer::GaussianSplat::Settings{};

    Renderer::Features::Settings& features = Renderer::Features::settings();
    features = Renderer::Features::Settings{};
    features.global_illumination = false;
    features.volumetrics = false;
    features.gaussian_splat = false;
}

} // namespace Rendering
