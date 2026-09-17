#include "Showcase.hpp"

#include <Models/Models.hpp>
#include <Renderer/Components.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

namespace Showcase {
namespace {

void appendReport(std::string *report, const std::string& message)
{
    if (!report) return;
    if (!report->empty()) report->append("\n");
    report->append(message);
}

bool add(
    Ecs::World& world,
    const Discovery::Placement& placement,
    Lineup& lineup,
    std::string *report,
    double *load_ms,
    double *instantiate_ms
)
{
    using Clock = std::chrono::steady_clock;

    std::string error;
    const auto load_begin = Clock::now();
    const Models::ModelHandle model = Models::load(placement.model.string(), &error);
    if (load_ms) {
        *load_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - load_begin
        ).count();
    }

    if (model == Models::INVALID_MODEL) {
        appendReport(
            report,
            "[GAME] [SHOWCASE] Skipped " + placement.model.string() + ": " + error
        );
        return false;
    }

    const Ecs::Entity root = world.createEntity();
    world.add<Renderer::Transform>(root, Renderer::Transform{
        .position = {placement.x, 10.0f, placement.z},
    });

    Renderer::ModelScene::Instance instance;
    const Renderer::ModelScene::Options options{
        .parent = root,
        .instantiate_cameras = false,
        .instantiate_lights = false,
    };

    const auto instantiate_begin = Clock::now();
    const bool instantiated = Renderer::ModelScene::instantiate(
        world,
        model,
        &instance,
        options,
        &error
    );
    if (instantiate_ms) {
        *instantiate_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - instantiate_begin
        ).count();
    }

    if (!instantiated) {
        if (world.alive(root)) world.destroyEntity(root);
        appendReport(
            report,
            "[GAME] [SHOWCASE] Skipped " + placement.model.string() + ": " + error
        );
        return false;
    }

    lineup.roots.push_back(root);
    lineup.instances.push_back(std::move(instance));
    return true;
}

} // namespace

void prepare(const std::filesystem::path& cs2_root, Loader& loader)
{
    loader.items = Discovery::lineup(cs2_root);
    loader.next = 0u;
    loader.loaded = 0u;
    std::printf("[GAME] [SHOWCASE] Queued %zu models\n", loader.items.size());
}

bool step(
    Ecs::World& world,
    Loader& loader,
    Lineup& lineup,
    std::string *report
)
{
    if (complete(loader)) return false;

    const Discovery::Placement& placement = loader.items[loader.next];
    double load_ms = 0.0;
    double instantiate_ms = 0.0;
    const bool loaded = add(
        world,
        placement,
        lineup,
        report,
        &load_ms,
        &instantiate_ms
    );

    ++loader.next;
    if (loaded) ++loader.loaded;

    std::printf(
        "[GAME] [SHOWCASE] %zu/%zu %s load=%.2fms instantiate=%.2fms%s\n",
        loader.next,
        loader.items.size(),
        placement.model.filename().string().c_str(),
        load_ms,
        instantiate_ms,
        loaded ? "" : " FAILED"
    );

    if (complete(loader)) {
        std::printf(
            "[GAME] [SHOWCASE] Loaded %zu/%zu models\n",
            loader.loaded,
            loader.items.size()
        );
        if (report && !report->empty()) std::fprintf(stderr, "%s\n", report->c_str());
    }

    return true;
}

bool complete(const Loader& loader)
{
    return loader.next >= loader.items.size();
}

void destroy(Ecs::World& world, Lineup& lineup)
{
    for (auto& instance : lineup.instances)
        Renderer::ModelScene::destroy(world, instance);

    for (const Ecs::Entity root : lineup.roots)
        if (root != Ecs::INVALID_ENTITY && world.alive(root)) world.destroyEntity(root);

    lineup.instances.clear();
    lineup.roots.clear();
}

} // namespace Showcase
