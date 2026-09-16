#include <Window/Window.hpp>
#include <Input/Input.hpp>

#include <Ecs/Ecs.hpp>

#include <Camera/Camera.hpp>

#include <Models/Models.hpp>
#include <Models/Runtime.hpp>

#include <Renderer/Components.hpp>
#include <Renderer/Environment.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>
#include <Renderer/Scenes/SceneCache.hpp>

#include <UI/UI.hpp>

#include "Debugging/Debugging.hpp"

#include <filesystem>
#include <chrono>
#include <cstdio>
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
            .fov_degrees = 70.0f,
            .active = true,
        });

        Camera::FreeController camera_controller;
        camera_controller.setSpeed(10.0f);
        camera_controller.setSprintMultiplier(10.0f);
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

        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .position = {0.0f, 1.0f, 0.0f},
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type = Renderer::LightType::Point,
            .color = {1.0f, 1.0f, 1.0f},
            .intensity = 1.0f,
            .range = 200.0f,
        });

        world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{});

        const Ecs::Entity environment = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(environment, Renderer::EnvironmentComponent{});

        std::string error;

        const std::filesystem::path PATH = "Assets/";
        
        const Models::ModelHandle scene = Models::load(
            (PATH / "Sponza/sponza.obj").string(),
            &error
        );

        const Models::ModelHandle arms = Models::load(
            (PATH / "CS2/Arms/agents/models/shared/arms/glove_fullfinger/glove_fullfinger.gltf").string(),
            &error
        );
        const Models::ModelHandle weapon = Models::load(
            (PATH / "CS2/Models/weapons/models/revolver/weapon_pist_revolver.gltf").string(),
            &error
        );
        const Models::ModelHandle idle = Models::load(
            (PATH / "CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/idle_revolver.gltf").string(),
            &error
        );
        const Models::ModelHandle shoot = Models::load(
            (PATH / "CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/shoot1_revolver.gltf").string(),
            &error
        );
        const Models::ModelHandle reload = Models::load(
            (PATH / "CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/reload_revolver.gltf").string(),
            &error
        );
        const Models::ModelHandle inspect = Models::load(
            (PATH / "CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/lookat01_revolver.gltf").string(),
            &error
        );
        
        bool valid = (arms == Models::INVALID_MODEL ||
                    weapon == Models::INVALID_MODEL ||
                     scene == Models::INVALID_MODEL ||
                      idle == Models::INVALID_MODEL ||
                     shoot == Models::INVALID_MODEL || 
                    reload == Models::INVALID_MODEL ||
                   inspect == Models::INVALID_MODEL);

        if (valid) {
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not load assets: ") + error + "\n";
        }

        const Renderer::ModelScene::Options viewmodel_options{
            .parent = viewmodel
        };

        Renderer::ModelScene::Instance scene_instance;
        if (!Renderer::ModelScene::instantiate(world, scene, &scene_instance)) {
            Renderer::ModelScene::destroy(world, scene_instance);
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not instantiate sponza-scene") + error + "\n";
        }

        Renderer::ModelScene::Instance arms_instance;
        if (!Renderer::ModelScene::instantiate(world, arms, &arms_instance, viewmodel_options, &error)) {
            Renderer::ModelScene::destroy(world, arms_instance);
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not instantiate CS2 arm-viewmodel: ") + error + "\n";
        }

        Renderer::ModelScene::Instance weapon_instance;
        if (!Renderer::ModelScene::instantiate(world, weapon, &weapon_instance, viewmodel_options, &error)) {
            Renderer::ModelScene::destroy(world, weapon_instance);
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not instantiate CS2 world-viewmodel: ") + error + "\n";
        }

        Renderer::ModelScene::Animation animation;
        const std::size_t arms_target = Renderer::ModelScene::bind(
            animation,
            arms_instance,
            Models::Runtime::RetargetOptions{
                .mode = Models::Runtime::RetargetMode::World,
                .source_root = "root_motion",
            }
        );
        const std::size_t weapon_target = Renderer::ModelScene::bind(
            animation, weapon_instance
        );

        valid = (arms_target == Models::INVALID_INDEX || 
               weapon_target == Models::INVALID_INDEX ||
            !Renderer::ModelScene::attach(animation, weapon_target, "wpn", "weapon") ||
            !Renderer::ModelScene::play(animation, idle, 0u, true, &error));

        if (valid) {
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Window::destroy();
            return std::string("[GAME] [ERROR] Could not bind CS2 animations: ") + error + "\n";
        }

        Renderer::Scenes::SceneCache::setMaximumTriangles(
            std::numeric_limits<std::size_t>::max()
        );

        Renderer::Rasterizer renderer;
        renderer.setEnabled(true);
        renderer.setViewportCulling(true);

        if (!renderer.init()) {
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Window::destroy();
            return "[GAME] [ERROR] Could not init rasterizer\n";
        }

        int width  = Window::width();
        int height = Window::height();
        renderer.resize(width, height);

        if (!UI::init()) {
            renderer.shutdown();
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Window::destroy();
            return "[GAME] [ERROR] Could not init debug UI\n";
        }

        Debugging::State debugging;
        Debugging::applyStyle();

        const Input::Button shoot_button = Input::button("left");
        const Input::Key reload_key = Input::key("R");
        const Input::Key inspect_key = Input::key("F");
        const Input::Key capture_key = Input::key("Tab");
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
            
            if (Input::keyPressed(capture_key)) {
                Input::setPointerCaptured(!Input::pointer().captured);
            }

            const auto update_begin = Clock::now();

            if (Input::pointer().captured) {
                camera_controller.update(world, delta);

                if (Input::buttonPressed(shoot_button) &&
                    !Renderer::ModelScene::play(animation, shoot, 0u, false, &error)) {
                    break;
                }

                if (Input::keyPressed(reload_key) &&
                    !Renderer::ModelScene::play(animation, reload, 0u, false, &error)) {
                    break;
                }

                if (Input::keyPressed(inspect_key) &&
                    !Renderer::ModelScene::play(animation, inspect, 0u, false, &error)) {
                    break;
                }
            }

            if (!Renderer::ModelScene::update(world, animation, delta, &error)) {
                break;
            }

            if (!Renderer::ModelScene::playing(animation) &&
                !Renderer::ModelScene::play(animation, idle, 0u, true, &error)) {
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

            Debugging::Position camera_position{};
            if (const Renderer::Transform *transform = world.get<Renderer::Transform>(camera)) {
                camera_position = {
                    transform->position.x,
                    transform->position.y,
                    transform->position.z,
                };
            }

            const auto ui_begin = Clock::now();
            Debugging::draw(debugging, camera_position);
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
        renderer.shutdown();
        Renderer::ModelScene::destroy(world, weapon_instance);
        Renderer::ModelScene::destroy(world, arms_instance);
        Input::reset();
        Window::destroy();
        return "[GAME] [FINISHED]";
    }
};

int main()
{
    return GAME::start() == "[GAME] [FINISHED]" ? 0 : 1;
}
