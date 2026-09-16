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

#include <chrono>
#include <cstdio>
#include <limits>
#include <string>

class GAME
{
public:
    static int start()
    {
        Window::Settings settings{};
        settings.title = "GAME";
        settings.width = 1280;
        settings.height = 720;
        if (!Window::create(settings)) return 1;

        Ecs::World world;

        const Ecs::Entity camera = world.createEntity();
        world.add<Renderer::Transform>(camera, Renderer::Transform{
            .position = {0.0f, 6.0f, 0.0f},
        });
        world.add<Camera::CameraComponent>(camera, Camera::CameraComponent{
            .fov_degrees = 70.0f,
            .near_plane = 0.01f,
            .active = true,
            .projection = Camera::Projection::Perspective,
            .far_plane = 1000.0f,
        });

        Camera::FreeController camera_controller;
        camera_controller.setSpeed(5.0f);
        camera_controller.setSprintMultiplier(2.0f);
        camera_controller.setMouseSensitivity(0.1f);
        camera_controller.setPitchRange(-89.0f, 89.0f);

        const Ecs::Entity viewmodel = world.createEntity();
        world.add<Renderer::Transform>(viewmodel, Renderer::Transform{
            .scale = {1.0f, 1.0f, -1.0f},
        });
        world.add<Renderer::Parent>(viewmodel, Renderer::Parent{camera});

        const Ecs::Entity light = world.createEntity();
        world.add<Renderer::Transform>(light, Renderer::Transform{
            .rotation = {35.0f, -30.0f, 0.0f},
        });
        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
            .type = Renderer::LightType::Directional,
            .color = {1.0f, 1.0f, 1.0f},
            .intensity = 3.0f,
        });
        world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{});

        const Ecs::Entity environment = world.createEntity();
        world.add<Renderer::EnvironmentComponent>(environment, Renderer::EnvironmentComponent{
            .sky_color = {0.1f, 0.1f, 0.2f},
            .intensity = 0.8f,
            .ambient_color = {1.0f, 1.0f, 1.0f},
            .ambient_intensity = 0.25f,
        });

        std::string error;
        const Models::ModelHandle sponza = Models::load("Assets/Sponza/sponza.obj", &error);
        const Models::ModelHandle arms = Models::load(
            "Assets/CS2/Arms/agents/models/shared/arms/glove_fullfinger/glove_fullfinger.gltf",
            &error
        );
        const Models::ModelHandle weapon = Models::load(
            "Assets/CS2/Models/weapons/models/revolver/weapon_pist_revolver.gltf",
            &error
        );
        const Models::ModelHandle idle = Models::load(
            "Assets/CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/idle_revolver.gltf",
            &error
        );
        const Models::ModelHandle shoot = Models::load(
            "Assets/CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/shoot1_revolver.gltf",
            &error
        );
        const Models::ModelHandle reload = Models::load(
            "Assets/CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/reload_revolver.gltf",
            &error
        );
        const Models::ModelHandle inspect = Models::load(
            "Assets/CS2/Anim/animation/anims/viewmodel/pistol/pistol_revolver/lookat01_revolver.gltf",
            &error
        );

        if (sponza == Models::INVALID_MODEL || arms == Models::INVALID_MODEL ||
            weapon == Models::INVALID_MODEL || idle == Models::INVALID_MODEL ||
            shoot == Models::INVALID_MODEL || reload == Models::INVALID_MODEL ||
            inspect == Models::INVALID_MODEL) {
            std::printf("[GAME] [ERROR] Could not load assets: %s\n", error.c_str());
            Window::destroy();
            return 1;
        }

        const Ecs::Entity sponza_root = world.createEntity();
        world.add<Renderer::Transform>(sponza_root, Renderer::Transform{
            .position = {0.0f, 1.2644f, 0.0f},
            .scale = {0.01f, 0.01f, 0.01f},
        });

        Renderer::ModelScene::Instance sponza_instance;
        if (!Renderer::ModelScene::instantiate(
                world,
                sponza,
                &sponza_instance,
                Renderer::ModelScene::Options{.parent = sponza_root},
                &error)) {
            std::printf("[GAME] [ERROR] Could not instantiate Sponza: %s\n", error.c_str());
            Window::destroy();
            return 1;
        }

        Renderer::ModelScene::Instance arms_instance;
        Renderer::ModelScene::Instance weapon_instance;
        const Renderer::ModelScene::Options viewmodel_options{.parent = viewmodel};
        if (!Renderer::ModelScene::instantiate(world, arms, &arms_instance, viewmodel_options, &error) ||
            !Renderer::ModelScene::instantiate(world, weapon, &weapon_instance, viewmodel_options, &error)) {
            std::printf("[GAME] [ERROR] Could not instantiate CS2 viewmodel: %s\n", error.c_str());
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Renderer::ModelScene::destroy(world, sponza_instance);
            Window::destroy();
            return 1;
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
        const std::size_t weapon_target = Renderer::ModelScene::bind(animation, weapon_instance);
        if (arms_target == Models::INVALID_INDEX || weapon_target == Models::INVALID_INDEX ||
            !Renderer::ModelScene::attach(animation, weapon_target, "wpn", "weapon") ||
            !Renderer::ModelScene::play(animation, idle, 0u, true, &error)) {
            std::printf("[GAME] [ERROR] Could not bind CS2 animations: %s\n", error.c_str());
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Renderer::ModelScene::destroy(world, sponza_instance);
            Window::destroy();
            return 1;
        }

        Renderer::Scenes::SceneCache::setMaximumTriangles(
            std::numeric_limits<std::size_t>::max()
        );

        Renderer::Rasterizer renderer;
        renderer.setEnabled(true);
        renderer.setViewportCulling(true);
        renderer.setClearColor({0.0f, 0.0f, 0.0f, 1.0f});
        if (!renderer.init()) {
            std::printf("[GAME] [ERROR] Could not init rasterizer\n");
            Renderer::ModelScene::destroy(world, weapon_instance);
            Renderer::ModelScene::destroy(world, arms_instance);
            Renderer::ModelScene::destroy(world, sponza_instance);
            Window::destroy();
            return 1;
        }

        int width = Window::width();
        int height = Window::height();
        renderer.resize(width, height);

        const Input::Button shoot_button = Input::button("left");
        const Input::Key reload_key = Input::key("R");
        const Input::Key inspect_key = Input::key("F");
        const Input::Key capture_key = Input::key("Tab");
        Input::setPointerCaptured(true);

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();
        int result = 0;

        while (Window::poll()) {
            Input::poll();

            const auto now = Clock::now();
            float delta = std::chrono::duration<float>(now - previous).count();
            previous = now;
            if (delta > 0.1f) delta = 0.1f;

            if (Input::keyPressed(capture_key))
                Input::setPointerCaptured(!Input::pointer().captured);
            if (Input::pointer().captured) camera_controller.update(world, delta);

            if (Input::buttonPressed(shoot_button) &&
                !Renderer::ModelScene::play(animation, shoot, 0u, false, &error)) {
                result = 1;
                break;
            }
            if (Input::keyPressed(reload_key) &&
                !Renderer::ModelScene::play(animation, reload, 0u, false, &error)) {
                result = 1;
                break;
            }
            if (Input::keyPressed(inspect_key) &&
                !Renderer::ModelScene::play(animation, inspect, 0u, false, &error)) {
                result = 1;
                break;
            }

            if (!Renderer::ModelScene::update(world, animation, delta, &error)) {
                result = 1;
                break;
            }
            if (!Renderer::ModelScene::playing(animation) &&
                !Renderer::ModelScene::play(animation, idle, 0u, true, &error)) {
                result = 1;
                break;
            }

            const int new_width = Window::width();
            const int new_height = Window::height();
            if (new_width != width || new_height != height) {
                width = new_width;
                height = new_height;
                renderer.resize(width, height);
            }

            renderer.render(world);
        }

        if (result != 0 && !error.empty())
            std::printf("[GAME] [ERROR] Animation failed: %s\n", error.c_str());

        renderer.shutdown();
        Renderer::ModelScene::destroy(world, weapon_instance);
        Renderer::ModelScene::destroy(world, arms_instance);
        Renderer::ModelScene::destroy(world, sponza_instance);
        Input::reset();
        Window::destroy();
        return result;
    }
};

int main()
{
    return GAME::start();
}
