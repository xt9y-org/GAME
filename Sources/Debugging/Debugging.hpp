#ifndef GAME_DEBUGGING_DEBUGGING_HPP
#define GAME_DEBUGGING_DEBUGGING_HPP

#include <Camera/FreeController.hpp>
#include <Ecs/Ecs.hpp>

#include <array>
#include <cstddef>

namespace Renderer {
class Rasterizer;
}

namespace Loadout {
struct State;
}

namespace Debugging {

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

    bool show_fps = true;
    bool show_camera = false;
};

struct Context
{
    Ecs::World& world;
    Camera::FreeController& camera_controller;
    Renderer::Rasterizer& renderer;
    Loadout::State& loadout;
    Ecs::Entity camera = Ecs::INVALID_ENTITY;
    Ecs::Entity environment = Ecs::INVALID_ENTITY;
    Ecs::Entity light = Ecs::INVALID_ENTITY;
    int width = 1;
    int height = 1;
};

void applyStyle();
void sample(State& state, float delta_seconds);
void draw(State& state, Context& context);

} // namespace Debugging

#endif
