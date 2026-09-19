#ifndef GAME_DEBUGGING_LAYOUT_HPP
#define GAME_DEBUGGING_LAYOUT_HPP

#include <Renderer/Features.hpp>

namespace Debugging::Layout {

inline constexpr float ControlWidth = 220.0f;
inline constexpr float PanelWidth = 350.0f;
inline constexpr float PanelPadding = 8.0f;
inline constexpr float PanelSpacing = 8.0f;
inline constexpr float PerformanceHeight = 360.0f;
inline constexpr float GlobalIlluminationPerformanceHeight = 88.0f;
inline constexpr float CameraHeight = 190.0f;

constexpr float performanceHeight(bool global_illumination)
{
    return PerformanceHeight +
        (global_illumination ? GlobalIlluminationPerformanceHeight : 0.0f);
}

constexpr float CameraPanelTop(
    bool show_fps,
    bool global_illumination,
    float menu_height)
{
    return menu_height + PanelPadding +
        (show_fps ? performanceHeight(global_illumination) + PanelSpacing : 0.0f);
}

inline float CameraPanelTop(bool show_fps, float menu_height)
{
    return CameraPanelTop(
        show_fps,
        Renderer::Features::currentSettings().global_illumination,
        menu_height
    );
}

} // namespace Debugging::Layout

#endif
