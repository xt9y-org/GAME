#include "Rendering/Setup.hpp"

#include <Renderer/Manager.hpp>
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

    assert(!Renderer::Volumetrics::currentSettings().enabled);

    return 0;
}
