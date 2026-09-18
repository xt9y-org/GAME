#include <Window/Window.hpp>
#include <Input/Input.hpp>

#include <Ecs/Ecs.hpp>

#include <Camera/Camera.hpp>

#include <Renderer/Components.hpp>
#include <Renderer/Environment.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Scenes/SceneCache.hpp>

#include <UI/UI.hpp>

#include "Debugging/Debugging.hpp"
#include "Debugging/Values.hpp"
#include "Loadout/Firing.hpp"
#include "Loadout/Loadout.hpp"
#include "Viewmodel/Motion.hpp"

#include <chrono>
#include <filesystem>
#include <limits>
#include <string>

class GAME
{
public:
    static std::string start()
    {
        Window::Settings settings{};
        if (!Window::create(settings)) return "[GAME] [ERROR] Could not create window\n";

        Ecs::World world;

        const Ecs::Entity camera = world.createEntity();
        world.add<Renderer::Transform>(camera, Renderer::Transform{});
        world.add<Camera::CameraComponent>(camera, Camera::CameraComponent{
            .fov_degrees = Debugging::Values::CameraFovDefault,
            .near_plane  = Debugging::Values::CameraNearDefault,
            .active      = true,
            .far_plane   = Debugging::Values::CameraFarDefault,
        });

        Camera::FreeController camera_controller;
        camera_controller.setSpeed(45.0f);
        camera_controller.setSprintMultiplier(50.0f);
        camera_controller.setMouseSensitivity(0.1f);
        camera_controller.setPitchRange(-89.0f, 89.0f);

        const Ecs::Entity viewmodel = world.createEntity();
        world.add<Renderer::Transform>(viewmodel, Renderer::Transform{
            .scale = {1.0f, 1.0f, -1.0f},
        });
        world.add<Renderer::Parent>(viewmodel, Renderer::Parent{camera});
        world.add<Renderer::RenderLayerComponent>(viewmodel, Renderer::RenderLayerComponent{
            .layer = Renderer::RenderLayer::Overlay,
        });

        Viewmodel::Motion::Settings motion_settings;
        Viewmodel::Motion::State motion_state;

        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .rotation = {
                Debugging::Values::LightRotationXDefault,
                Debugging::Values::LightRotationYDefault,
                Debugging::Values::LightRotationZDefault,
            },
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type       = Renderer::LightType::Directional,
            .color      = {1.0f, 1.0f, 1.0f},
            .intensity  = Debugging::Values::LightIntensityDefault,
            .range      = 200.0f,
        });
        world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{
            .enabled = true,
            .bias = Debugging::Values::ShadowBiasDefault,
        });

        const Ecs::Entity environment = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(
            environment,
            Renderer::EnvironmentComponent{}
        );

        std::string error;
        Loadout::State loadout;
        if (!Loadout::init(loadout, world, viewmodel, "Assets/CS2", &error)) {
            Loadout::destroy(loadout, world);
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not load default loadout: ") + error + "\n";
        }
        Loadout::Firing::Controller firing;

        Renderer::Scenes::SceneCache::setMaximumTriangles(
            std::numeric_limits<std::size_t>::max()
        );

        Renderer::Rasterizer renderer;
        renderer.setEnabled(true);
        renderer.setViewportCulling(true);
        renderer.setShadowResolution(Debugging::Values::ShadowResolutionDefault);
        renderer.setShadowCascades(Debugging::Values::ShadowCascadesDefault);
        renderer.setShadowDistance(Debugging::Values::ShadowDistanceDefault);
        renderer.setShadowNearPlane(Debugging::Values::ShadowNearDefault);
        renderer.setClearColor({0.0f, 0.0f, 0.0f, 0.0f});

        if (!renderer.init()) {
            Loadout::destroy(loadout, world);
            Window::destroy();
            return "[GAME] [ERROR] Could not init rasterizer\n";
        }

        int width  = Window::width();
        int height = Window::height();
        renderer.resize(width, height);

        if (!UI::init()) {
            renderer.shutdown();
            Loadout::destroy(loadout, world);
            Window::destroy();
            return "[GAME] [ERROR] Could not init debug UI\n";
        }

        Debugging::State debugging;
        Debugging::applyStyle();

        const Input::Button shoot_button = Input::button("left");
        const Input::Button alternate_fire_button = Input::button("right");
        const Input::Key reload_key = Input::key("R");
        const Input::Key inspect_key = Input::key("F");
        const Input::Key capture_key = Input::key("Tab");
        const Input::Key forward_key = Input::key("W");
        const Input::Key backward_key = Input::key("S");
        const Input::Key left_key = Input::key("A");
        const Input::Key right_key = Input::key("D");
        const Input::Key sprint_key = Input::key("Left Shift");
        Input::setPointerCaptured(true);

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();

        while (Window::poll()) {
            Input::poll();

            const auto now = Clock::now();
            float delta = std::chrono::duration<float>(now - previous).count();
            previous = now;
            if (delta > 0.1f) delta = 0.1f;
            Debugging::sample(debugging, delta);

            if (Input::keyPressed(capture_key))
                Input::setPointerCaptured(!Input::pointer().captured);

            const auto update_begin = Clock::now();
            const Input::Pointer pointer = Input::pointer();

            if (pointer.captured)
                camera_controller.update(world, delta);

            if (!Loadout::Firing::update(
                    firing,
                    loadout,
                    pointer.captured && Input::buttonPressed(shoot_button),
                    pointer.captured && Input::buttonDown(shoot_button),
                    pointer.captured && Input::buttonPressed(alternate_fire_button),
                    delta,
                    &error))
                break;

            if (pointer.captured) {
                if (Input::keyPressed(reload_key) &&
                    !Loadout::reload(loadout, &error))
                    break;

                if (Input::keyPressed(inspect_key) &&
                    !Loadout::inspect(loadout, &error))
                    break;
            }

            if (!Loadout::update(loadout, world, delta, &error))
                break;

            const Renderer::Transform *camera_transform =
                world.get<Renderer::Transform>(camera);
            if (!camera_transform) {
                error = "Could not read camera transform for viewmodel motion";
                break;
            }

            Viewmodel::Motion::Sample motion_sample;
            motion_sample.camera_pitch = camera_transform->rotation.x;
            motion_sample.action_active =
                loadout.animation.active && loadout.animation.model != loadout.idle;

            if (pointer.captured) {
                motion_sample.mouse_dx = static_cast<float>(pointer.dx);
                motion_sample.mouse_dy = static_cast<float>(pointer.dy);
                motion_sample.move_x =
                    (Input::keyDown(right_key) ? 1.0f : 0.0f) -
                    (Input::keyDown(left_key) ? 1.0f : 0.0f);
                motion_sample.move_y =
                    (Input::keyDown(forward_key) ? 1.0f : 0.0f) -
                    (Input::keyDown(backward_key) ? 1.0f : 0.0f);
                motion_sample.sprint = Input::keyDown(sprint_key);
            }

            const Viewmodel::Motion::Pose motion_pose = Viewmodel::Motion::step(
                motion_state,
                motion_settings,
                motion_sample,
                delta
            );
            if (!Viewmodel::Motion::apply(world, viewmodel, motion_pose)) {
                error = "Could not apply viewmodel motion";
                break;
            }

            const int new_width  = Window::width();
            const int new_height = Window::height();

            if (new_width != width || new_height != height) {
                width = new_width;
                height = new_height;
                renderer.resize(width, height);
            }

            debugging.update_ms = std::chrono::duration<float, std::milli>(
                Clock::now() - update_begin
            ).count();

            if (!UI::beginFrame() && !UI::initialized()) {
                error = "Could not begin debug UI frame";
                break;
            }
            UI::showOverlay();

            Debugging::Context debug_context{
                world,
                camera_controller,
                renderer,
                loadout,
                camera,
                environment,
                light,
                width,
                height,
            };

            const auto ui_begin = Clock::now();
            Debugging::draw(debugging, debug_context);
            debugging.ui_ms = std::chrono::duration<float, std::milli>(
                Clock::now() - ui_begin
            ).count();

            const auto render_begin = Clock::now();
            renderer.render(world);
            debugging.render_ms = std::chrono::duration<float, std::milli>(
                Clock::now() - render_begin
            ).count();
        }

        UI::shutdown();
        Loadout::destroy(loadout, world);
        renderer.shutdown();
        Input::reset();
        Window::destroy();
        return "[GAME] [FINISHED]";
    }
};

int main()
{
    return GAME::start() == "[GAME] [FINISHED]" ? 0 : 1;
}
