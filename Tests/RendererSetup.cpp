#include "Rendering/Setup.hpp"

#include <Renderer/Features.hpp>
#include <Renderer/Manager.hpp>
#include <Renderer/Quality.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Volumetrics/Volumetrics.hpp>

#include <cassert>

int main()
{
    Renderer::Manager renderers;
    Rendering::configure(renderers);

    assert(renderers.count() == 1u);
    assert(renderers.entry(0u));
    assert(renderers.entry(0u)->name == "Rasterizer");

    Renderer::Rasterizer *rasterizer = renderers.find<Renderer::Rasterizer>();
    assert(rasterizer);
    assert(rasterizer->enabled());
    assert(rasterizer->shadowQuality() == Renderer::Quality::High);

    const Renderer::Features::Settings& features = Renderer::Features::currentSettings();
    assert(features.lighting);
    assert(features.shadows);
    assert(features.environment);
    assert(!features.global_illumination);
    assert(!features.volumetrics);
    assert(!features.gaussian_splat);

    assert(Renderer::Volumetrics::currentSettings().enabled);

    return 0;
}
