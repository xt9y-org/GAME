#ifndef GAME_DEBUGGING_VALUES_HPP
#define GAME_DEBUGGING_VALUES_HPP

#include <algorithm>
#include <array>
#include <cstddef>

namespace Debugging::Values {

template <typename T>
struct Range
{
    T minimum;
    T maximum;
    T step;
};

template <typename T>
constexpr T stepValue(T value, T step, T minimum, T maximum, int direction)
{
    return std::clamp(value + step * static_cast<T>(direction), minimum, maximum);
}

inline constexpr float CameraFovDefault = 70.0f;
inline constexpr float CameraNearDefault = 0.1f;
inline constexpr float CameraFarDefault = 0.0f;
inline constexpr int ShadowResolutionDefault = 512;
inline constexpr int ShadowCascadesDefault = 4;
inline constexpr float ShadowDistanceDefault = 1600.0f;
inline constexpr float ShadowNearDefault = 0.05f;
inline constexpr float LightIntensityDefault = 1.0f;
inline constexpr float LightRotationXDefault = 0.45f;
inline constexpr float LightRotationYDefault = 0.45f;
inline constexpr float LightRotationZDefault = 0.45f;
inline constexpr float ShadowBiasDefault = 0.002f;

inline constexpr Range<float> CameraFov{30.0f, 120.0f, 1.0f};
inline constexpr Range<float> CameraNear{0.01f, 2.0f, 0.01f};
inline constexpr Range<float> CameraFar{0.0f, 10000.0f, 50.0f};
inline constexpr Range<float> OrthographicSize{0.1f, 1000.0f, 0.1f};
inline constexpr Range<float> CameraSpeed{0.0f, 200.0f, 1.0f};
inline constexpr Range<float> SprintMultiplier{1.0f, 100.0f, 1.0f};
inline constexpr Range<float> MouseSensitivity{0.01f, 1.0f, 0.01f};
inline constexpr Range<float> OverlayOpacity{0.0f, 1.0f, 0.05f};
inline constexpr Range<int> ShadowCascades{1, 4, 1};
inline constexpr Range<float> ShadowDistance{10.0f, 5000.0f, 50.0f};
inline constexpr Range<float> ShadowNear{0.01f, 1.0f, 0.01f};
inline constexpr Range<float> EnvironmentIntensity{0.0f, 10.0f, 0.1f};
inline constexpr Range<float> EnvironmentRotation{-180.0f, 180.0f, 1.0f};
inline constexpr Range<float> AmbientIntensity{0.0f, 1.0f, 0.05f};
inline constexpr Range<float> FogDensity{0.0f, 1.0f, 0.001f};
inline constexpr Range<float> FogDistance{0.0f, 10000.0f, 10.0f};
inline constexpr Range<float> DirectionalIntensity{0.0f, 10.0f, 0.1f};
inline constexpr Range<float> LocalLightIntensity{0.0f, 100000.0f, 10.0f};
inline constexpr Range<float> LocalLightRange{0.0f, 10000.0f, 10.0f};
inline constexpr Range<float> ConeAngle{0.0f, 179.0f, 1.0f};
inline constexpr Range<float> ShadowBias{0.0f, 0.05f, 0.0001f};

inline constexpr std::array<int, 6> ShadowResolutions{
    128, 256, 512, 1024, 2048, 4096
};

constexpr std::size_t shadowResolutionIndex(int value)
{
    std::size_t best = 0;
    int best_distance = value > ShadowResolutions[0]
        ? value - ShadowResolutions[0]
        : ShadowResolutions[0] - value;

    for (std::size_t i = 1; i < ShadowResolutions.size(); ++i) {
        const int distance = value > ShadowResolutions[i]
            ? value - ShadowResolutions[i]
            : ShadowResolutions[i] - value;
        if (distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

constexpr int stepShadowResolution(int value, int direction)
{
    const std::size_t index = shadowResolutionIndex(value);
    if (direction < 0 && index > 0) return ShadowResolutions[index - 1];
    if (direction > 0 && index + 1 < ShadowResolutions.size()) return ShadowResolutions[index + 1];
    return ShadowResolutions[index];
}

} // namespace Debugging::Values

#endif
