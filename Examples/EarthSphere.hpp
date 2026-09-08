#ifndef GAME_EXAMPLES_EARTH_SPHERE_HPP
#define GAME_EXAMPLES_EARTH_SPHERE_HPP

#include "Sources/Models/Models.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace EarthDemo {

inline constexpr std::size_t kLongitudeSegments = 256u;
inline constexpr std::size_t kLatitudeSegments = 128u;
inline constexpr float kPi = 3.14159265358979323846f;

inline Models::MeshData makeSphere(float radius)
{
    Models::MeshData mesh;
    if (!(radius > 0.0f)) return mesh;

    const std::size_t stride = kLongitudeSegments + 1u;
    mesh.vertices.reserve((kLatitudeSegments + 1u) * stride);
    mesh.indices.reserve(kLongitudeSegments * (kLatitudeSegments - 1u) * 6u);

    for (std::size_t latitude = 0; latitude <= kLatitudeSegments; ++latitude)
    {
        const float v = static_cast<float>(latitude) /
            static_cast<float>(kLatitudeSegments);
        const float theta = v * kPi;
        const float sin_theta = std::sin(theta);
        const float cos_theta = std::cos(theta);

        for (std::size_t longitude = 0; longitude <= kLongitudeSegments; ++longitude)
        {
            const float u = static_cast<float>(longitude) /
                static_cast<float>(kLongitudeSegments);
            const float phi = (u - 0.5f) * (2.0f * kPi);

            const float nx = sin_theta * std::sin(phi);
            const float ny = cos_theta;
            const float nz = sin_theta * std::cos(phi);

            Models::Vertex vertex;
            vertex.position = {nx * radius, ny * radius, nz * radius};
            vertex.normal = {nx, ny, nz};
            // Horse flips image Y at sampling time, so V=1 is the north pole.
            vertex.uv = {u, 1.0f - v};
            mesh.vertices.push_back(vertex);
        }
    }

    for (std::size_t latitude = 0; latitude < kLatitudeSegments; ++latitude)
    {
        const std::size_t row0 = latitude * stride;
        const std::size_t row1 = (latitude + 1u) * stride;

        for (std::size_t longitude = 0; longitude < kLongitudeSegments; ++longitude)
        {
            const std::uint32_t a = static_cast<std::uint32_t>(row0 + longitude);
            const std::uint32_t b = static_cast<std::uint32_t>(row0 + longitude + 1u);
            const std::uint32_t c = static_cast<std::uint32_t>(row1 + longitude);
            const std::uint32_t d = static_cast<std::uint32_t>(row1 + longitude + 1u);

            if (latitude == 0u)
            {
                // The north-pole row intentionally duplicates the pole once per
                // longitude so each cap triangle owns its local U coordinate.
                mesh.indices.push_back(b);
                mesh.indices.push_back(c);
                mesh.indices.push_back(d);
                continue;
            }

            if (latitude + 1u == kLatitudeSegments)
            {
                mesh.indices.push_back(a);
                mesh.indices.push_back(c);
                mesh.indices.push_back(b);
                continue;
            }

            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);

            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    mesh.bounds.minimum = {-radius, -radius, -radius};
    mesh.bounds.maximum = { radius,  radius,  radius};
    return mesh;
}

} // namespace EarthDemo

#endif
