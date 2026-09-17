#include "Showcase.hpp"

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

bool instantiate(
    Ecs::World& world,
    const Discovery::Placement& placement,
    Models::ModelHandle model,
    Lineup& lineup,
    std::string *report,
    double *instantiate_ms
)
{
    using Clock = std::chrono::steady_clock;

    std::string error;
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

void printComplete(const Loader& loader, std::string *report)
{
    if (!complete(loader)) return;
    std::printf(
        "[GAME] [SHOWCASE] Loaded %zu/%zu models\n",
        loader.loaded,
        loader.items.size()
    );
    if (report && !report->empty()) std::fprintf(stderr, "%s\n", report->c_str());
}

} // namespace

void prepare(const std::filesystem::path& cs2_root, Loader& loader)
{
    loader.items = Discovery::lineup(cs2_root);
    loader.requests.clear();
    loader.requests.reserve(loader.items.size());
    for (const Discovery::Placement& placement : loader.items)
        loader.requests.push_back(Models::loadAsync(placement.model.string()));
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
    if (loader.next >= loader.requests.size()) {
        appendReport(report, "[GAME] [SHOWCASE] Async request list is incomplete");
        loader.next = loader.items.size();
        printComplete(loader, report);
        return true;
    }

    const Discovery::Placement& placement = loader.items[loader.next];
    const Models::LoadHandle request = loader.requests[loader.next];
    const Models::LoadState state = Models::loadState(request);
    if (state == Models::LoadState::Pending) return false;

    bool loaded = false;
    double instantiate_ms = 0.0;
    std::string error;

    if (state == Models::LoadState::Ready) {
        const Models::ModelHandle model = Models::loadResult(request, &error);
        if (model != Models::INVALID_MODEL) {
            loaded = instantiate(
                world,
                placement,
                model,
                lineup,
                report,
                &instantiate_ms
            );
        } else {
            appendReport(
                report,
                "[GAME] [SHOWCASE] Skipped " + placement.model.string() + ": " + error
            );
        }
    } else {
        Models::loadResult(request, &error);
        appendReport(
            report,
            "[GAME] [SHOWCASE] Skipped " + placement.model.string() + ": " +
                (error.empty() ? std::string("asynchronous model load failed") : error)
        );
    }

    ++loader.next;
    if (loaded) ++loader.loaded;

    std::printf(
        "[GAME] [SHOWCASE] %zu/%zu %s instantiate=%.2fms%s\n",
        loader.next,
        loader.items.size(),
        placement.model.filename().string().c_str(),
        instantiate_ms,
        loaded ? "" : " FAILED"
    );

    printComplete(loader, report);
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
