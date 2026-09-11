#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Models/Runtime.hpp"
#include "Sources/Renderer/Environment.hpp"
#include "Sources/Renderer/Math.hpp"
#include "Sources/Renderer/Render.hpp"

#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

constexpr int initial_tile_count = 18;
constexpr int tile_guard = 3;

class Application {
public:
    ~Application()
    {
        shutdown();
    }

    int run()
    {
        if (!init()) return 1;
        if (!load()) return 2;

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();

        while (!Display.isCloseRequested()) {
            Display.processMessages();
            if (Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) break;

            const auto now = Clock::now();
            const float delta_seconds = std::min(
                std::chrono::duration<float>(now - previous).count(),
                0.1f
            );
            previous = now;

            camera_controller_.update(world_, delta_seconds);
            expandRoad();
            resizeIfNeeded();
            renderer_.render(world_);
        }

        return 0;
    }

private:
    bool init()
    {
        lwcglInstallFastRuntime();
#ifdef __APPLE__
        lwcglSetContextVersion(2, 1);
        lwcglSetContextProfile(LWCGL_CONTEXT_ANY_PROFILE);
#else
        lwcglSetContextVersion(4, 3);
        lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

        Display.setDisplayMode(new DisplayMode(1280, 720));
        Display.create();
        Display.setTitle("GAME");
        started_ = true;

        Keyboard.create();
        Mouse.create();

        renderer_.setEnabled(true);
        renderer_.setViewportCulling(false);
        renderer_.setShadowResolution(1024);
        renderer_.setFallbackShadowResolution(256);
        renderer_.setMinimumShadowResolution(64);
        renderer_.setShadowNearPlane(0.05f);
        renderer_.setShadowFarScale(1.05f);
        renderer_.setClearColor({0.38f, 0.50f, 0.66f, 1.0f});

        if (!renderer_.init()) {
            std::fprintf(stderr, "failed to initialize rasterizer\n");
            return false;
        }

        framebuffer_width_ = std::max(Display.getWidth(), 1);
        framebuffer_height_ = std::max(Display.getHeight(), 1);
        renderer_.resize(framebuffer_width_, framebuffer_height_);
        return true;
    }

    bool load()
    {
        road_model_ = Models::load("Assets/road_tile.glb", &model_error_);
        if (road_model_ == Models::INVALID_MODEL) {
            std::fprintf(
                stderr,
                "failed to load Assets/road_tile.glb: %s\n",
                model_error_.empty() ? "unknown error" : model_error_.c_str()
            );
            return false;
        }

        if (!Models::Runtime::reset(road_model_, &road_pose_, &model_error_)) {
            std::fprintf(
                stderr,
                "failed to resolve road_tile.glb node transforms: %s\n",
                model_error_.empty() ? "unknown error" : model_error_.c_str()
            );
            return false;
        }

        if (!measureRoad()) {
            std::fprintf(stderr, "road_tile.glb contains no usable mesh\n");
            return false;
        }

        camera_ = world_.createEntity();
        world_.add<Renderer::Transform>(camera_, Renderer::Transform{
            .position = {0.0f, 3.0f, 0.0f},
            .rotation = {-12.0f, stack_on_x_ ? 90.0f : 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world_.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
            78.0f, 0.05f, true
        });

        camera_controller_.setSpeed(16.0f);
        camera_controller_.setSprintMultiplier(10.0f);
        camera_controller_.setMouseSensitivity(0.10f);
        camera_controller_.setPitchRange(-89.0f, 89.0f);

        const Ecs::Entity environment = world_.createEntity();
        world_.add<Renderer::EnvironmentComponent>(
            environment,
            Renderer::EnvironmentComponent{
                .enabled = true,
                .texture = Models::INVALID_TEXTURE,
                .sky_color = {0.38f, 0.50f, 0.66f},
                .intensity = 1.0f,
                .rotation_degrees = 0.0f,
                .ambient_color = {1.0f, 1.0f, 1.0f},
                .ambient_intensity = 0.65f,
                .fog = Renderer::FogMode::None,
            }
        );

        minimum_tile_index_ = -(initial_tile_count / 2);
        maximum_tile_index_ = minimum_tile_index_ + initial_tile_count - 1;
        for (long long index = minimum_tile_index_; index <= maximum_tile_index_; ++index) {
            if (!createRoadTile(index)) {
                std::fprintf(stderr, "road_tile.glb contains no renderable parts\n");
                return false;
            }
        }

        world_.markChanged();
        return true;
    }

    Models::Mat4 partMatrix(const Models::ModelPart& part) const
    {
        if (part.node != Models::INVALID_INDEX && part.node < road_pose_.nodes.size())
            return road_pose_.nodes[part.node].world;
        return Models::identityMatrix();
    }

    bool measureRoad()
    {
        const float infinity = std::numeric_limits<float>::infinity();
        road_bounds_.minimum = {infinity, infinity, infinity};
        road_bounds_.maximum = {-infinity, -infinity, -infinity};

        bool found = false;
        const std::size_t parts = Models::partCount(road_model_);
        for (std::size_t index = 0; index < parts; ++index) {
            const Models::ModelPart *part = Models::part(road_model_, index);
            if (!part) continue;
            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh) continue;

            const Models::Mat4 matrix = partMatrix(*part);
            const Models::Bounds& bounds = mesh->bounds;
            const std::array<Renderer::Vec3, 8> corners {{
                {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
                {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
                {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
                {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
                {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
                {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
                {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
                {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
            }};

            for (const Renderer::Vec3 corner : corners) {
                const Renderer::Vec3 point = Renderer::Math::transformPoint(matrix, corner);
                road_bounds_.minimum.x = std::min(road_bounds_.minimum.x, point.x);
                road_bounds_.minimum.y = std::min(road_bounds_.minimum.y, point.y);
                road_bounds_.minimum.z = std::min(road_bounds_.minimum.z, point.z);
                road_bounds_.maximum.x = std::max(road_bounds_.maximum.x, point.x);
                road_bounds_.maximum.y = std::max(road_bounds_.maximum.y, point.y);
                road_bounds_.maximum.z = std::max(road_bounds_.maximum.z, point.z);
                found = true;
            }
        }

        if (!found) return false;

        const float width = road_bounds_.maximum.x - road_bounds_.minimum.x;
        const float depth = road_bounds_.maximum.z - road_bounds_.minimum.z;
        stack_on_x_ = width >= depth;
        tile_length_ = stack_on_x_ ? width : depth;
        if (tile_length_ <= 0.001f) return false;

        model_offset_ = {
            -(road_bounds_.minimum.x + road_bounds_.maximum.x) * 0.5f,
            -road_bounds_.minimum.y,
            -(road_bounds_.minimum.z + road_bounds_.maximum.z) * 0.5f,
        };
        return true;
    }

    Renderer::Vec3 tilePosition(long long index) const
    {
        const float offset = static_cast<float>(index) * tile_length_;
        Renderer::Vec3 position = model_offset_;
        if (stack_on_x_) position.x += offset;
        else position.z += offset;
        return position;
    }

    bool createRoadTile(long long index)
    {
        const Ecs::Entity root = world_.createEntity();
        world_.add<Renderer::Transform>(root, Renderer::Transform{
            .position = tilePosition(index),
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });

        bool attached = false;
        const std::size_t parts = Models::partCount(road_model_);
        for (std::size_t part_index = 0; part_index < parts; ++part_index) {
            const Models::ModelPart *part = Models::part(road_model_, part_index);
            if (!part || part->mesh == Models::INVALID_MESH) continue;

            Renderer::Transform authored_transform{};
            authored_transform.matrix_override = partMatrix(*part);
            authored_transform.matrix_override_enabled = true;

            const Ecs::Entity child = world_.createEntity();
            world_.add<Renderer::Transform>(child, authored_transform);
            world_.add<Renderer::Parent>(child, Renderer::Parent{root});
            world_.add<Renderer::MeshComponent>(
                child,
                Renderer::MeshComponent{part->mesh, part->material}
            );
            world_.add<Renderer::RenderableComponent>(
                child,
                Renderer::RenderableComponent{true}
            );
            attached = true;
        }
        return attached;
    }

    void expandRoad()
    {
        if (road_expansion_failed_) return;

        const Renderer::Transform *camera_transform = world_.get<Renderer::Transform>(camera_);
        if (!camera_transform) return;

        const float camera_axis = stack_on_x_
            ? camera_transform->position.x
            : camera_transform->position.z;
        const long long camera_index = static_cast<long long>(
            std::floor(camera_axis / tile_length_)
        );

        bool changed = false;
        while (camera_index < minimum_tile_index_ + tile_guard) {
            const long long next = minimum_tile_index_ - 1;
            if (!createRoadTile(next)) {
                road_expansion_failed_ = true;
                break;
            }
            minimum_tile_index_ = next;
            changed = true;
        }

        while (!road_expansion_failed_ && camera_index > maximum_tile_index_ - tile_guard) {
            const long long next = maximum_tile_index_ + 1;
            if (!createRoadTile(next)) {
                road_expansion_failed_ = true;
                break;
            }
            maximum_tile_index_ = next;
            changed = true;
        }

        if (road_expansion_failed_)
            std::fprintf(stderr, "failed to extend infinite road\n");
        if (changed) world_.markChanged();
    }

    void resizeIfNeeded()
    {
        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);
        if (width == framebuffer_width_ && height == framebuffer_height_) return;

        framebuffer_width_ = width;
        framebuffer_height_ = height;
        renderer_.resize(width, height);
    }

    void shutdown()
    {
        if (!started_) return;
        renderer_.shutdown();
        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        started_ = false;
    }

    bool started_ = false;
    bool stack_on_x_ = false;
    bool road_expansion_failed_ = false;
    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;
    long long minimum_tile_index_ = 0;
    long long maximum_tile_index_ = 0;

    Ecs::World world_;
    Renderer::Rasterizer renderer_;
    Camera::FreeController camera_controller_;
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;

    Models::ModelHandle road_model_ = Models::INVALID_MODEL;
    Models::Runtime::Pose road_pose_{};
    Models::Bounds road_bounds_{};
    Renderer::Vec3 model_offset_{};
    float tile_length_ = 1.0f;
    std::string model_error_;
};

} // namespace

int main()
{
    Application application;
    return application.run();
}
