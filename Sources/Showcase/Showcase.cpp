#include "Showcase.hpp"
#include "Discovery.hpp"

#include <Models/Models.hpp>
#include <Renderer/Components.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace Showcase {
namespace {

struct Row
{
    std::vector<std::filesystem::path> models;
    float z = 0.0f;
    float spacing = 50.0f;
};

void appendReport(std::string *report, const std::string& message)
{
    if (!report) return;
    if (!report->empty()) report->append("\n");
    report->append(message);
}

bool add(
    Ecs::World& world,
    const std::filesystem::path& model_path,
    float x,
    float z,
    Lineup& lineup,
    std::string *report
)
{
    std::string error;
    const Models::ModelHandle model = Models::load(model_path.string(), &error);
    if (model == Models::INVALID_MODEL) {
        appendReport(report, "[GAME] [SHOWCASE] Skipped " + model_path.string() + ": " + error);
        return false;
    }

    const Ecs::Entity root = world.createEntity();
    world.add<Renderer::Transform>(root, Renderer::Transform{
        .position = {x, 10.0f, z},
    });

    Renderer::ModelScene::Instance instance;
    const Renderer::ModelScene::Options options{
        .parent = root,
        .instantiate_cameras = false,
        .instantiate_lights = false,
    };

    if (!Renderer::ModelScene::instantiate(world, model, &instance, options, &error)) {
        if (world.alive(root)) world.destroyEntity(root);
        appendReport(report, "[GAME] [SHOWCASE] Skipped " + model_path.string() + ": " + error);
        return false;
    }

    lineup.roots.push_back(root);
    lineup.instances.push_back(std::move(instance));
    return true;
}

void appendPrimary(
    std::vector<std::filesystem::path>& models,
    const std::filesystem::path& directory
)
{
    const std::filesystem::path model = Discovery::primaryModel(directory);
    if (!model.empty()) models.push_back(model);
}

} // namespace

std::size_t create(
    Ecs::World& world,
    const std::filesystem::path& cs2_root,
    Lineup& lineup,
    std::string *report
)
{
    destroy(world, lineup);
    if (report) report->clear();

    const std::filesystem::path weapons = cs2_root / "Models/weapons/models";
    const std::filesystem::path arms = cs2_root / "Arms/agents/models/shared/arms";

    std::vector<std::filesystem::path> firearms = Discovery::childModels(
        weapons,
        {"knife", "grenade", "shared", "c4", "defuser", "healthshot"}
    );

    std::vector<std::filesystem::path> knives = Discovery::childModels(
        weapons / "knife"
    );

    std::vector<std::filesystem::path> equipment;
    appendPrimary(equipment, weapons / "c4");
    appendPrimary(equipment, weapons / "defuser");
    appendPrimary(equipment, weapons / "healthshot");
    const auto grenades = Discovery::childModels(weapons / "grenade", {"shared"});
    equipment.insert(equipment.end(), grenades.begin(), grenades.end());

    std::vector<std::filesystem::path> gloves = Discovery::childModels(
        arms,
        {"glove_cloth_collision"}
    );

    const std::array<Row, 4> rows{{
        {std::move(firearms), -250.0f, 55.0f},
        {std::move(knives), -350.0f, 45.0f},
        {std::move(equipment), -450.0f, 55.0f},
        {std::move(gloves), -550.0f, 60.0f},
    }};

    std::size_t created = 0;
    for (const Row& row : rows) {
        for (std::size_t i = 0; i < row.models.size(); ++i) {
            if (add(
                world,
                row.models[i],
                Discovery::centeredX(i, row.models.size(), row.spacing),
                row.z,
                lineup,
                report
            )) ++created;
        }
    }

    return created;
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
