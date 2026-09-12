#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Camera.hpp"
#include "Renderer/Components.hpp"

namespace Tests {
namespace {

class ProjectionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "camera/projection"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity first = world.createEntity();
        world.add<Renderer::Transform>(first, Renderer::Transform{});
        world.add<Camera::CameraComponent>(first, Camera::CameraComponent{
            .fov_degrees = 60.0f, .near_plane = 0.1f, .active = false,
            .projection = Camera::Projection::Perspective, .far_plane = 500.0f,
        });
        const Ecs::Entity second = world.createEntity();
        world.add<Renderer::Transform>(second, Renderer::Transform{});
        world.add<Camera::CameraComponent>(second, Camera::CameraComponent{
            .near_plane = 0.01f, .active = true, .projection = Camera::Projection::Orthographic,
            .far_plane = 100.0f, .xmag = 4.0f, .ymag = 3.0f,
        });
        return Testing::require(Camera::activeCamera(world) == second, "active camera selection mismatch", error) &&
            Testing::require(world.get<Camera::CameraComponent>(second)->projection == Camera::Projection::Orthographic,
                             "orthographic projection lost", error);
    }
};

class ControllerCase final : public Testing::Case {
public:
    std::string_view name() const override { return "camera/controller"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Camera::FreeController controller;
        controller.setSpeed(3.5f);
        controller.setSprintMultiplier(2.0f);
        controller.setMouseSensitivity(0.25f);
        controller.setPitchRange(-70.0f, 80.0f);
        const Renderer::Vec3 forward = Camera::flightDirection(0.0f, 0.0f);
        const Renderer::Vec3 right = Camera::strafeDirection(0.0f);
        return Testing::require(Testing::near(controller.speed(), 3.5f), "controller speed mismatch", error) &&
            Testing::require(Testing::near(controller.sprintMultiplier(), 2.0f), "sprint multiplier mismatch", error) &&
            Testing::require(Testing::near(controller.mouseSensitivity(), 0.25f), "mouse sensitivity mismatch", error) &&
            Testing::require(Testing::near(controller.minimumPitch(), -70.0f) && Testing::near(controller.maximumPitch(), 80.0f),
                             "pitch range mismatch", error) &&
            Testing::require(Testing::near(forward.x * right.x + forward.y * right.y + forward.z * right.z, 0.0f, 1.0e-3f),
                             "camera forward/right are not orthogonal", error);
    }
};

} // namespace

void registerCamera(Testing::Runner& runner)
{
    runner.add<ProjectionCase>();
    runner.add<ControllerCase>();
}

} // namespace Tests
