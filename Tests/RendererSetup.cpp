#include "Rendering/Setup.hpp"

#include <Renderer/Manager.hpp>
#include <Renderer/PathTracer/PathTracer.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/RayTracer/RayTracer.hpp>
#include <Renderer/Volumetrics/Volumetrics.hpp>

#include <cassert>

int main()
{
    Renderer::Manager renderers;
    Rendering::configure(renderers);

    assert(renderers.count() == 3u);
    assert(renderers.entry(0u));
    assert(renderers.entry(1u));
    assert(renderers.entry(2u));
    assert(renderers.entry(0u)->name == "Rasterizer");
    assert(renderers.entry(1u)->name == "Ray Tracer");
    assert(renderers.entry(2u)->name == "Path Tracer");

    Renderer::Rasterizer *rasterizer = renderers.find<Renderer::Rasterizer>();
    Renderer::RayTracer *ray_tracer = renderers.find<Renderer::RayTracer>();
    Renderer::PathTracer *path_tracer = renderers.find<Renderer::PathTracer>();

    assert(rasterizer);
    assert(ray_tracer);
    assert(path_tracer);
    assert(rasterizer->enabled());

    assert(ray_tracer->enabled());
    assert(ray_tracer->reconstructionSettings().quality == 0.85f);
    assert(ray_tracer->reconstructionSettings().maximum_history == 32u);
    assert(ray_tracer->reconstructionSettings().temporal_reuse);
    assert(!ray_tracer->reconstructionSettings().debug_reconstruction);

    assert(path_tracer->enabled());
    assert(path_tracer->samplesPerFrame() == 1);
    assert(path_tracer->reconstructionSettings().quality == 0.85f);
    assert(path_tracer->reconstructionSettings().maximum_history == 32u);
    assert(path_tracer->reconstructionSettings().temporal_reuse);
    assert(!path_tracer->reconstructionSettings().debug_reconstruction);

    assert(!Renderer::Volumetrics::currentSettings().enabled);

    return 0;
}
