#include "Examples/EarthSphere.hpp"

#include <cmath>
#include <cstddef>

namespace {

bool near(float a, float b, float epsilon = 1.0e-4f)
{
    return std::fabs(a - b) <= epsilon;
}

float length(const Models::Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

int main()
{
    constexpr float radius = 3.0f;
    const Models::MeshData mesh = EarthDemo::makeSphere(radius);

    if (mesh.vertices.size() < 30000u) return 2;
    if (mesh.indices.size() / 3u < 60000u) return 3;
    if (mesh.indices.size() % 3u != 0u) return 4;

    if (!near(mesh.bounds.minimum.x, -radius) || !near(mesh.bounds.maximum.x, radius)) return 5;
    if (!near(mesh.bounds.minimum.y, -radius) || !near(mesh.bounds.maximum.y, radius)) return 6;
    if (!near(mesh.bounds.minimum.z, -radius) || !near(mesh.bounds.maximum.z, radius)) return 7;

    for (const Models::Vertex& vertex : mesh.vertices) {
        if (!near(length(vertex.position), radius, 2.0e-4f)) return 8;
        if (!near(length(vertex.normal), 1.0f, 2.0e-4f)) return 9;
        if (vertex.uv.x < 0.0f || vertex.uv.x > 1.0f) return 10;
        if (vertex.uv.y < 0.0f || vertex.uv.y > 1.0f) return 11;
    }

    // Longitude seam vertices must duplicate position/normal while carrying
    // U=0 and U=1 so interpolation never crosses the entire texture.
    const std::size_t stride = EarthDemo::kLongitudeSegments + 1u;
    const std::size_t middle_ring = EarthDemo::kLatitudeSegments / 2u;
    const Models::Vertex& seam0 = mesh.vertices[middle_ring * stride];
    const Models::Vertex& seam1 = mesh.vertices[middle_ring * stride + EarthDemo::kLongitudeSegments];
    if (!near(seam0.position.x, seam1.position.x) ||
        !near(seam0.position.y, seam1.position.y) ||
        !near(seam0.position.z, seam1.position.z)) return 12;
    if (!near(seam0.uv.x, 0.0f) || !near(seam1.uv.x, 1.0f)) return 13;

    return 0;
}
