#include "Driving/Assets.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

namespace Game::Driving::Assets {
namespace {

Model loadModel(const char *path, Library& library)
{
    ++library.requested;
    if (!path || !std::filesystem::exists(path)) return {};
    ++library.files_present;

    std::string error;
    const Models::ModelHandle handle = Models::load(path, &error);
    if (handle == Models::INVALID_MODEL) return {};

    const float infinity = std::numeric_limits<float>::infinity();
    Models::Bounds bounds{
        .minimum = {infinity, infinity, infinity},
        .maximum = {-infinity, -infinity, -infinity},
    };

    bool has_mesh = false;
    const std::size_t parts = Models::partCount(handle);
    for (std::size_t index = 0u; index < parts; ++index) {
        const Models::ModelPart *part = Models::part(handle, index);
        if (!part) continue;
        const Models::MeshData *mesh = Models::mesh(part->mesh);
        if (!mesh) continue;

        bounds.minimum.x = std::min(bounds.minimum.x, mesh->bounds.minimum.x);
        bounds.minimum.y = std::min(bounds.minimum.y, mesh->bounds.minimum.y);
        bounds.minimum.z = std::min(bounds.minimum.z, mesh->bounds.minimum.z);
        bounds.maximum.x = std::max(bounds.maximum.x, mesh->bounds.maximum.x);
        bounds.maximum.y = std::max(bounds.maximum.y, mesh->bounds.maximum.y);
        bounds.maximum.z = std::max(bounds.maximum.z, mesh->bounds.maximum.z);
        has_mesh = true;
    }

    if (!has_mesh) return {};

    const float width = bounds.maximum.x - bounds.minimum.x;
    const float depth = bounds.maximum.z - bounds.minimum.z;

    ++library.loaded;
    return Model{
        .handle = handle,
        .bounds = bounds,
        .yaw_degrees = width > depth ? 90.0f : 0.0f,
    };
}

void addIfValid(std::vector<Model>& models, Model model)
{
    if (model.valid()) models.push_back(model);
}

} // namespace

void Library::load()
{
    cars.clear();
    heavy_traffic.clear();
    street.clear();
    foliage.clear();
    city.clear();
    requested = 0u;
    files_present = 0u;
    loaded = 0u;

    addIfValid(cars, loadModel(
        "Assets/Driving/Cars/1967_chevy_camaro_ss_hidden_jewel.glb", *this));
    addIfValid(cars, loadModel(
        "Assets/Driving/Cars/2010_mercedes-benz_sls_amg.glb", *this));
    addIfValid(cars, loadModel(
        "Assets/Driving/Cars/2015_mercedes-benz_s65_amg_coupe.glb", *this));

    addIfValid(street, loadModel(
        "Assets/Driving/Street/low_poly_street_gameready_6.glb", *this));
    addIfValid(street, loadModel(
        "Assets/Driving/Street/road_signs_asset_pack__australian_american.glb", *this));

    addIfValid(foliage, loadModel(
        "Assets/Driving/Foliage/low_poly_stylized_plants_pack_free.glb", *this));

    addIfValid(city, loadModel(
        "Assets/Driving/City/street_city_7_for_games_free.glb", *this));
    addIfValid(city, loadModel(
        "Assets/Driving/City/street_city_buildings_8.glb", *this));
}

std::size_t attach(
    Ecs::World& world,
    Ecs::Entity parent,
    const Model& model,
    float target_length,
    float yaw_offset_degrees)
{
    if (!model.valid() || !world.alive(parent)) return 0u;

    const float width = model.bounds.maximum.x - model.bounds.minimum.x;
    const float depth = model.bounds.maximum.z - model.bounds.minimum.z;
    const float horizontal_extent = std::max(std::max(width, depth), 0.001f);
    const float scale = std::max(target_length, 0.001f) / horizontal_extent;
    const float yaw = model.yaw_degrees + yaw_offset_degrees;
    const float radians = yaw * (3.14159265358979323846f / 180.0f);
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    const float center_x = (model.bounds.minimum.x + model.bounds.maximum.x) * 0.5f;
    const float center_z = (model.bounds.minimum.z + model.bounds.maximum.z) * 0.5f;
    const float rotated_center_x = cosine * center_x + sine * center_z;
    const float rotated_center_z = -sine * center_x + cosine * center_z;

    const Renderer::Transform local{
        .position = {
            -rotated_center_x * scale,
            -model.bounds.minimum.y * scale,
            -rotated_center_z * scale,
        },
        .rotation = {0.0f, yaw, 0.0f},
        .scale = {scale, scale, scale},
    };

    std::size_t triangle_count = 0u;
    const std::size_t parts = Models::partCount(model.handle);
    for (std::size_t index = 0u; index < parts; ++index) {
        const Models::ModelPart *part = Models::part(model.handle, index);
        if (!part || part->mesh == Models::INVALID_MESH) continue;

        const Ecs::Entity child = world.createEntity();
        world.add<Renderer::Transform>(child, local);
        world.add<Renderer::Parent>(child, Renderer::Parent{parent});
        world.add<Renderer::MeshComponent>(
            child,
            Renderer::MeshComponent{part->mesh, part->material});
        world.add<Renderer::RenderableComponent>(
            child,
            Renderer::RenderableComponent{true});

        const Models::MeshData *mesh = Models::mesh(part->mesh);
        if (mesh) triangle_count += mesh->indices.size() / 3u;
    }

    return triangle_count;
}

} // namespace Game::Driving::Assets
