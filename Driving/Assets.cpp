#include "Driving/Assets.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace Game::Driving::Assets {
namespace {

constexpr const char *highway_directory = "Assets/Driving/Street/Highway";
constexpr float pi = 3.14159265358979323846f;

std::string lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); }
    );
    return value;
}

bool supportedModel(const std::filesystem::path& path)
{
    const std::string extension = lower(path.extension().string());
    return extension == ".glb" || extension == ".gltf" ||
        extension == ".fbx" || extension == ".obj";
}

int modelPriority(const std::filesystem::path& path)
{
    const std::string name = lower(path.filename().string());
    if (name.find("highway") != std::string::npos) return 0;
    if (name.find("road") != std::string::npos) return 1;
    if (name.find("street") != std::string::npos) return 2;
    return 3;
}

std::vector<std::filesystem::path> highwayModels()
{
    std::vector<std::filesystem::path> result;
    std::error_code error;
    const std::filesystem::path root(highway_directory);
    if (!std::filesystem::exists(root, error) || error) return result;

    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error
    );
    const std::filesystem::recursive_directory_iterator end;

    while (iterator != end) {
        if (!error && iterator->is_regular_file(error) && !error && supportedModel(iterator->path()))
            result.push_back(iterator->path());

        error.clear();
        iterator.increment(error);
        if (error) error.clear();
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            const int a_priority = modelPriority(a);
            const int b_priority = modelPriority(b);
            if (a_priority != b_priority) return a_priority < b_priority;
            return a.generic_string() < b.generic_string();
        }
    );
    return result;
}

Model loadModel(const std::filesystem::path& path, std::string& error)
{
    std::string model_error;
    const Models::ModelHandle handle = Models::load(path.string(), &model_error);
    if (handle == Models::INVALID_MODEL) {
        error = path.filename().string();
        if (!model_error.empty()) error += ": " + model_error;
        return {};
    }

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

    if (!has_mesh) {
        error = path.filename().string() + ": model contains no renderable mesh parts";
        return {};
    }

    const float width = bounds.maximum.x - bounds.minimum.x;
    const float depth = bounds.maximum.z - bounds.minimum.z;

    return Model{
        .handle = handle,
        .bounds = bounds,
        .yaw_degrees = width > depth ? 90.0f : 0.0f,
    };
}

} // namespace

bool Library::load(std::string& error)
{
    street = {};
    requested = 1u;
    files_present = 0u;
    loaded = 0u;
    error.clear();

    const std::vector<std::filesystem::path> candidates = highwayModels();
    files_present = candidates.size();
    if (candidates.empty()) {
        error = "realistic highway archive contains no supported model (.glb/.gltf/.fbx/.obj)";
        return false;
    }

    std::string first_error;
    for (const std::filesystem::path& path : candidates) {
        std::string candidate_error;
        Model model = loadModel(path, candidate_error);
        if (model.valid()) {
            street = model;
            loaded = 1u;
            return true;
        }
        if (first_error.empty()) first_error = std::move(candidate_error);
    }

    error = "failed to load a road model from realistic-road-highway-gameready.zip";
    if (!first_error.empty()) error += ": " + first_error;
    return false;
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
    const float radians = yaw * (pi / 180.0f);
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
            Renderer::MeshComponent{part->mesh, part->material}
        );
        world.add<Renderer::RenderableComponent>(
            child,
            Renderer::RenderableComponent{true}
        );

        const Models::MeshData *mesh = Models::mesh(part->mesh);
        if (mesh) triangle_count += mesh->indices.size() / 3u;
    }

    return triangle_count;
}

} // namespace Game::Driving::Assets
