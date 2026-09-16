#ifndef GAME_DEBUGGING_LAYOUT_HPP
#define GAME_DEBUGGING_LAYOUT_HPP

namespace Debugging::Layout {

inline constexpr float ControlWidth = 220.0f;
inline constexpr float PanelWidth = 350.0f;
inline constexpr float PanelPadding = 8.0f;
inline constexpr float PanelSpacing = 8.0f;
inline constexpr float PerformanceHeight = 360.0f;
inline constexpr float CameraHeight = 190.0f;

constexpr float CameraPanelTop(bool show_fps, float menu_height)
{
    return menu_height + PanelPadding +
        (show_fps ? PerformanceHeight + PanelSpacing : 0.0f);
}

} // namespace Debugging::Layout

#endif
