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
        if (Window::create(settings)) return 1;

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

        Ecs::Entity weapon_root = world.createEntity();

        world.add<Renderer::Transform>(weapon_root, Renderer::Transform{
                .position = {0.0f, -0.1f, -0.55f},
                .rotation = {0.0f,  0.0f,  0.0f},
                .scale    = {1.0f,  1.0f,  1.0f},
        });

        world.add<Renderer::Transform>(weapon_root, Renderer::Transform{camera});

        Ecs::Entity light = world.createEntity();

        world.add<Renderer::Transform>(light, Renderer::Transform{
                .position = {0.0f,   1.0f,  0.0f},
                .rotation = {35.0f, -30.0f, 0.0f},
        });

        std::string error;

        Models::ModelHandle arms = Models::load(
                "Assets/CS2/Arms/glove_fullfinger.gltf", &error
        );

        if (arms = Models::INVALID_MODEL) {
            std::cerr 
        }
        
        Models::ModelHandle revo = Models::load(
                "Assets/CS2/Revolover/weapon_pist_revolver.gltf", &error
        );

        Renderer::ModelScene::Instance();
            

        Window::destroy();
        return ret;
    }


private:

};

int main() {
    return GAME::start();
}
