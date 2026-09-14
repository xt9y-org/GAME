#include <Window/Window.hpp>
#include <Input/Input.hpp>

#include <Ecs/Ecs.hpp>

#include <Camera/Camera.hpp>

#include <Models/Models.hpp>
#include <Models/Runtime.hpp>

#include <Renderer/Components.hpp>
#include <Renderer/ModelScene.hpp>
#include <Renderer/Rasterizer/Rasterizer.hpp>


#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>


class GAME 
{
public:
    static int start() 
    {
        int ret = 0;
        
        Window::Settings settings{};
        settings.title = "GAME";
        settings.width = 1280;
        settings.height = 720;
        if (!Window::create(settings)) return 1;

        Ecs::World world;
        Ecs::Entity camera = world.createEntity();

        world.add<Renderer::Transform>(camera, Renderer::Transform{
                .position = {0.0f, 0.0f, 0.0f},
                .rotation = {0.0f, 0.0f, 0.0f},
                .scale    = {1.0f, 1.0f, 1.0f},
        });

        world.add<Camera::CameraComponent>(camera, Camera::CameraComponent{
                .fov_degrees = 70.0f,
                .near_plane  = 0.01f,
                .active      = true,
                .projection  = Camera::Projection::Perspective,
                .far_plane   = 1000.0f,
        });

        Ecs::Entity arms_root = world.createEntity();

        world.add<Renderer::Transform>(arms_root, Renderer::Transform{
                .position = {0.0f, -1.1f, -0.65f},
                .rotation = {0.0f,  0.0f,  0.0f},
                .scale    = {1.1f,  1.1f,  1.1f},
        });

        world.add<Renderer::Parent>(arms_root, Renderer::Parent{camera});

        Ecs::Entity revo_root = world.createEntity();

        world.add<Renderer::Transform>(revo_root, Renderer::Transform{
                .position = {0.0f, -0.1f, -0.55f},
                .rotation = {0.0f,  0.0f,  0.0f},
                .scale    = {1.0f,  1.0f,  1.0f},
        });

        world.add<Renderer::Parent>(revo_root, Renderer::Parent{camera});

        Ecs::Entity light = world.createEntity();

        world.add<Renderer::Transform>(light, Renderer::Transform{
                .position = {0.0f,   1.0f,  0.0f},
                .rotation = {35.0f, -30.0f, 0.0f},
        });

        world.add<Renderer::LightComponent>(light, Renderer::LightComponent{
                .type      = Renderer::LightType::Directional,
                .color     = {1.0f, 1.0f, 1.0f},
                .intensity = 3.0f,
        });

        std::string error;

        Models::ModelHandle arms = Models::load(
                "Assets/CS2/Arms/agents/models/shared/arms/glove_fullfinger/glove_fullfinger.gltf", &error
        );

        if (arms == Models::INVALID_MODEL) {
            std::printf("[GAME] [ERROR] Could not load Arms model: %s\n", error.c_str());
            Window::destroy();
            return 1;
        }
        
        Models::ModelHandle revo = Models::load(
                "Assets/CS2/Models/weapons/models/revolver/weapon_pist_revolver.gltf", &error
        );

        if (revo == Models::INVALID_MODEL) {
            std::printf("[GAME] [ERROR] Could not load Revo model: %s\n", error.c_str());
            Window::destroy();
            return 1;
        }

        Renderer::ModelScene::Instance arms_instance;
        Renderer::ModelScene::Instance revo_instance;
        
        if (!Renderer::ModelScene::instantiate(world, arms, &arms_instance)) {
            std::printf("[GAME] [ERROR] Could not instantiate Arm model\n");
            Window::destroy();
            return 1;
        }


        if (!Renderer::ModelScene::instantiate(world, revo, &revo_instance)) {
            std::printf("[GAME] [ERROR] Could not instantiate Revo model\n");
            Renderer::ModelScene::destroy(world, arms_instance);
            Window::destroy();
            return 1;
        }

        parentRoots(world, arms, arms_instance, arms_root);
        parentRoots(world, revo, revo_instance, revo_root);

        std::size_t arms_anim = animation(arms, "inspect_loop");
        std::size_t revo_anim = animation(revo, "inventory_inspect");

        float arms_time = 0.0f,
              revo_time = 0.0f;

        bool arms_loop = true,
             revo_loop = true;

        Renderer::Rasterizer renderer;
        renderer.setEnabled(true);

        renderer.setClearColor(Renderer::Vec4{0.0f, 0.0f, 0.0f, 1.0f});

        if (!renderer.init()) {
            std::printf("[GAME] [ERROR] Could not init rasterizer\n");
            Renderer::ModelScene::destroy(world, arms_instance);
            Renderer::ModelScene::destroy(world, revo_instance);
            Window::destroy();
            return 1;
        }
        
        int width  = Window::width();
        int height = Window::height();

        renderer.resize(width, height);

        const Input::Button button_shoot = Input::button("left");
        const Input::Key key_reload  = Input::key("R");
        const Input::Key key_inspect = Input::key("F");

        using Clock = std::chrono::steady_clock;
        auto prev   = Clock::now();

        while (Window::poll()) {
            Input::poll();

            const auto now = Clock::now();
            float delta = std::chrono::duration<float>(now - prev).count();
            prev = now;

            if (delta > 0.1f) delta = 0.1f;

            if (Input::buttonPressed(button_shoot)) {
                const std::size_t shoot = animation(revo, "shoot");

                if (shoot != Models::INVALID_INDEX) {
                    revo_anim = shoot;
                    revo_time = 0.0f;
                    revo_loop = false;
                }

                const std::size_t arms_shoot = animation(arms, "shoot");

                if (arms_shoot != Models::INVALID_INDEX) {
                    arms_anim = arms_shoot;
                    arms_time = 0.0f;
                    arms_loop = false;
                }
            }

            if (Input::keyPressed(key_reload)) {
                const std::size_t revo_reload = animation(revo, "reload");

                if (revo_reload != Models::INVALID_INDEX) {
                    revo_anim = revo_reload;
                    revo_time = 0.0f;
                    revo_loop = false;
                }

                const std::size_t arms_reload = animation(arms, "reload");

                if (arms_reload != Models::INVALID_INDEX)
                {
                    arms_anim = arms_reload;
                    arms_time = 0.0f;
                    arms_loop = false;
                } 

            }

            if (Input::keyPressed(key_inspect)) {
                const std::size_t revo_inspect = animation(revo, "inventory_inspect");

                if (revo_inspect != Models::INVALID_INDEX) {
                    revo_anim = revo_inspect;
                    revo_time = 0.0f;
                    revo_loop = false;
                }

                const std::size_t arms_inspect = animation(arms, "inspect_loop");

                if (arms_inspect != Models::INVALID_INDEX)
                {
                    arms_anim = arms_inspect;
                    arms_time = 0.0f;
                    arms_loop = false;
                } 
            }

            arms_time += delta;
            revo_time += delta;

            if (!revo_loop) {
                const float end = duration(revo, revo_anim);

                if (end > 0.0f && revo_time >= end) {
                    revo_anim = animation(revo, "inventory_inspect");
                    revo_time = 0.0f;
                    revo_loop = true;
                }
            }

            if (!arms_loop) {
                const float end = duration(arms, arms_anim);

                if (end > 0.0f && arms_time >= end) {
                    arms_anim = animation(arms, "inspect_loop");
                    arms_time = 0.0f;
                    arms_loop = true;
                }
            }

            apply(world, arms, arms_instance, arms_anim, arms_time, arms_loop);
            apply(world, revo, revo_instance, revo_anim, revo_time, revo_loop);
            
            const int new_width  = Window::width();
            const int new_height = Window::height();

            if (new_width != width || new_height != height) {
                width  = new_width;
                height = new_height;

                renderer.resize(width, height);
            }

            renderer.render(world);
        }

        Renderer::ModelScene::destroy(world, revo_instance);
        Renderer::ModelScene::destroy(world, arms_instance);

        renderer.shutdown();

        Input::reset();

        Window::destroy();
        return ret;
    }


private:
    static std::size_t animation(
        Models::ModelHandle model,
        std::string_view name
    )
    {
        for (std::size_t i = 0; i < Models::modelAnimationCount(model); ++i) {
            const Models::ModelAnimationData *clip = Models::modelAnimation(model, i);
            if (clip && clip->name == name)
                return i;
        }
        return Models::INVALID_INDEX;
    }

    static float duration(
        Models::ModelHandle model,
        std::size_t index
    )
    {
        if (index == Models::INVALID_INDEX)
            return 0.0f;

        const Models::ModelAnimationData *clip = Models::modelAnimation(model, index);
        return clip ? clip->duration : 0.0f;
    }

    static bool apply(
        Ecs::World& world,
        Models::ModelHandle model,
        Renderer::ModelScene::Instance& instance,
        std::size_t animation_index,
        float time,
        bool loop
    )
    {
        if (animation_index == Models::INVALID_INDEX)
            return true;

        Models::Runtime::Pose pose;
        std::string error;

        if (!Models::Runtime::sample(model, animation_index, time, loop, &pose, &error)) {
            std::printf("[GAME] [ERROR] Animation sample failed: %s\n", error.c_str());
            return false;
        }

        if (!Renderer::ModelScene::applyPose(world, instance, pose, &error)) {
            std::printf("[GAME] [ERROR] Animation pose failed: %s\n", error.c_str());
            return false;
        }

        return true;
    }

    static void parentRoots(
        Ecs::World& world,
        Models::ModelHandle model,
        Renderer::ModelScene::Instance& instance,
        Ecs::Entity parent
    )
    {
        for (std::size_t i = 0; i < instance.nodes.size(); ++i) {
            const Models::NodeData *node = Models::node(model, i);
            if (!node)
                continue;
            if (node->parent >= 0)
                continue;

            Ecs::Entity entity = instance.nodes[i].entity;
            if (entity == Ecs::INVALID_ENTITY)
                continue;

            world.add<Renderer::Parent>(entity, Renderer::Parent{parent});
        }
    }
};

int main() {
    return GAME::start();
}
