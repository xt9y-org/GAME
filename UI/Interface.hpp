#ifndef GAME_UI_INTERFACE_HPP
#define GAME_UI_INTERFACE_HPP

#include "Scenes/Manager.hpp"

#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Renderer/Debug/Debug.hpp"
#include "Sources/Renderer/Manager.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Game::UI {

class Interface {
public:
    void setApproximationWindow(float x, float y, float width, float height);
    void setSceneWindow(float x, float y, float width, float height);
    void setInformationWindow(float x, float y, float width, float height);
    void setDebugWindow(float x, float y, float width, float height);
    void setTooltip(float padding_x, float padding_y, float wrap_width);
    void setControlWidth(float width);

    void addIntControl(
        Renderer::IRenderer& renderer,
        std::string label,
        std::function<int()> read,
        std::function<void(int)> write,
        int minimum,
        int maximum,
        std::string help
    );

    void addFloatControl(
        Renderer::IRenderer& renderer,
        std::string label,
        std::function<float()> read,
        std::function<void(float)> write,
        float minimum,
        float maximum,
        float speed,
        std::string help
    );

    std::size_t draw(
        Ecs::World& world,
        Scenes::Manager& scenes,
        Renderer::Manager& renderers,
        Renderer::Debug::Inspector& inspector
    );

private:
    struct Layout {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Control {
        enum class Type { Integer, Float };
        Type type = Type::Integer;
        Renderer::IRenderer *renderer = nullptr;
        std::string label;
        std::string help;
        std::function<int()> read_int;
        std::function<void(int)> write_int;
        std::function<float()> read_float;
        std::function<void(float)> write_float;
        int int_minimum = 0;
        int int_maximum = 0;
        float float_minimum = 0.0f;
        float float_maximum = 0.0f;
        float speed = 0.0f;
    };

    void help(const char *text) const;
    void place(const Layout& layout) const;
    void rendererControls(Renderer::Manager& renderers);
    void approximation(Ecs::World& world, Renderer::Manager& renderers);
    std::size_t sceneManager(Scenes::Manager& scenes, Renderer::Manager& renderers);
    void information(Renderer::Debug::Inspector& inspector);
    void debug(
        Ecs::World& world,
        Renderer::Manager& renderers,
        Renderer::Debug::Inspector& inspector);

    Layout approximation_layout_{};
    Layout scene_layout_{};
    Layout information_layout_{};
    Layout debug_layout_{};
    float tooltip_padding_x_ = 0.0f;
    float tooltip_padding_y_ = 0.0f;
    float tooltip_wrap_width_ = 0.0f;
    float control_width_ = 0.0f;
    std::vector<Control> controls_;
};

} // namespace Game::UI

#endif
