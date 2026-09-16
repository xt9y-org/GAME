#ifndef GAME_DEBUGGING_DEBUGGING_HPP
#define GAME_DEBUGGING_DEBUGGING_HPP

#include <array>
#include <cstddef>

namespace Debugging {

struct Position
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct State
{
    static constexpr std::size_t FRAME_HISTORY = 180;

    std::array<float, FRAME_HISTORY> frame_ms{};
    std::size_t frame_index = 0;
    bool frame_history_filled = false;

    float frame_ms_current = 0.0f;
    float fps = 0.0f;
    float update_ms = 0.0f;
    float render_ms = 0.0f;
    float ui_ms = 0.0f;

    bool show_console = false;
    bool show_fps = true;
    bool show_position = false;
    bool show_imgui_demo = false;
};

void applyStyle();
void sample(State& state, float delta_seconds);
void draw(State& state, Position position);

} // namespace Debugging

#endif
