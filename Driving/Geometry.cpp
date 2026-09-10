#include "Driving/Geometry.hpp"

#include "Driving/Traffic.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Game::Driving::Geometry {
namespace {

void addQuad(
    Models::MeshData& mesh,
    Models::Vec3 a,
    Models::Vec3 b,
    Models::Vec3 c,
    Models::Vec3 d,
    Models::Vec3 normal)
{
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Models::Vertex{a, normal, {0.0f, 0.0f}, {}});
    mesh.vertices.push_back(Models::Vertex{b, normal, {1.0f, 0.0f}, {}});
    mesh.vertices.push_back(Models::Vertex{c, normal, {1.0f, 1.0f}, {}});
    mesh.vertices.push_back(Models::Vertex{d, normal, {0.0f, 1.0f}, {}});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
}

} // namespace

Models::MaterialHandle material(Models::Vec3 color)
{
    Models::MaterialData value;
    value.color = color;
    value.opacity = 1.0f;
    return Models::registerMaterial(std::move(value));
}

Models::MeshHandle road(float width, float length)
{
    const float half_width = width * 0.5f;
    const float half_length = length * 0.5f;
    Models::MeshData mesh;
    addQuad(
        mesh,
        {-half_width, 0.0f, half_length},
        { half_width, 0.0f, half_length},
        { half_width, 0.0f,-half_length},
        {-half_width, 0.0f,-half_length},
        {0.0f, 1.0f, 0.0f}
    );
    mesh.bounds = {{-half_width, 0.0f, -half_length}, {half_width, 0.0f, half_length}};
    return Models::registerMesh(std::move(mesh));
}

Models::MeshHandle laneMarkings(int lane_count, float lane_width, float length)
{
    Models::MeshData mesh;
    constexpr float dash_length = 4.0f;
    constexpr float gap_length = 8.0f;
    constexpr float line_width = 0.12f;
    const float half_length = length * 0.5f;

    for (int boundary = 1; boundary < lane_count; ++boundary) {
        const float x = laneCenter(boundary - 1, lane_count, lane_width) + lane_width * 0.5f;
        for (float z = -half_length; z < half_length; z += dash_length + gap_length) {
            const float z0 = z;
            const float z1 = std::min(z + dash_length, half_length);
            addQuad(
                mesh,
                {x - line_width * 0.5f, 0.012f, z1},
                {x + line_width * 0.5f, 0.012f, z1},
                {x + line_width * 0.5f, 0.012f, z0},
                {x - line_width * 0.5f, 0.012f, z0},
                {0.0f, 1.0f, 0.0f}
            );
        }
    }

    const float half_width = static_cast<float>(lane_count) * lane_width * 0.5f;
    mesh.bounds = {{-half_width, 0.012f, -half_length}, {half_width, 0.012f, half_length}};
    return Models::registerMesh(std::move(mesh));
}

Models::MeshHandle unitBox()
{
    Models::MeshData mesh;
    const float h = 0.5f;

    addQuad(mesh, {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h}, { 0.0f, 0.0f, 1.0f});
    addQuad(mesh, { h,-h,-h}, {-h,-h,-h}, {-h, h,-h}, { h, h,-h}, { 0.0f, 0.0f,-1.0f});
    addQuad(mesh, {-h,-h,-h}, {-h,-h, h}, {-h, h, h}, {-h, h,-h}, {-1.0f, 0.0f, 0.0f});
    addQuad(mesh, { h,-h, h}, { h,-h,-h}, { h, h,-h}, { h, h, h}, { 1.0f, 0.0f, 0.0f});
    addQuad(mesh, {-h, h, h}, { h, h, h}, { h, h,-h}, {-h, h,-h}, { 0.0f, 1.0f, 0.0f});
    addQuad(mesh, {-h,-h,-h}, { h,-h,-h}, { h,-h, h}, {-h,-h, h}, { 0.0f,-1.0f, 0.0f});

    mesh.bounds = {{-h,-h,-h}, {h,h,h}};
    return Models::registerMesh(std::move(mesh));
}

} // namespace Game::Driving::Geometry
