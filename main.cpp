#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Environment.hpp"
#include "Sources/Renderer/Render.hpp"

#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr int tile_count = 18;
constexpr int tile_guard = 3;
constexpr float pi = 3.14159265358979323846f;

struct Tile {
    Ecs::Entity entity = Ecs::INVALID_ENTITY;
    long long index = 0;
};

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
            recycleRoad();
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
        renderer_.setViewportCulling(true);
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

        if (!measureRoad()) {
            std::fprintf(stderr, "road_tile.glb contains no usable mesh\n");
            return false;
        }

        camera_ = world_.createEntity();
        world_.add<Renderer::Transform>(camera_, Renderer::Transform{
            .position = {0.0f, 3.0f, 0.0f},
            .rotation = {-12.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world_.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
            78.0f, 0.05f, true
        });

        camera_controller_.setSpeed(16.0f);
        camera_controller_.setSprintMultiplier(4.0f);
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

        tiles_.reserve(tile_count);
        const int first = -(tile_count / 2);
        for (int slot = 0; slot < tile_count; ++slot) {
            const long long index = static_cast<long long>(first + slot);
            const Ecs::Entity root = world_.createEntity();
            world_.add<Renderer::Transform>(root, Renderer::Transform{
                .position = {0.0f, 0.0f, static_cast<float>(index) * tile_length_},
                .rotation = {},
                .scale = {1.0f, 1.0f, 1.0f},
            });

            if (!attachRoad(root)) {
                std::fprintf(stderr, "road_tile.glb contains no renderable parts\n");
                return false;
            }
            tiles_.push_back(Tile{root, index});
        }

        world_.markChanged();
        return true;
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

            road_bounds_.minimum.x = std::min(road_bounds_.minimum.x, mesh->bounds.minimum.x);
            road_bounds_.minimum.y = std::min(road_bounds_.minimum.y, mesh->bounds.minimum.y);
            road_bounds_.minimum.z = std::min(road_bounds_.minimum.z, mesh->bounds.minimum.z);
            road_bounds_.maximum.x = std::max(road_bounds_.maximum.x, mesh->bounds.maximum.x);
            road_bounds_.maximum.y = std::max(road_bounds_.maximum.y, mesh->bounds.maximum.y);
            road_bounds_.maximum.z = std::max(road_bounds_.maximum.z, mesh->bounds.maximum.z);
            found = true;
        }

        if (!found) return false;

        const float width = road_bounds_.maximum.x - road_bounds_.minimum.x;
        const float depth = road_bounds_.maximum.z - road_bounds_.minimum.z;
        road_yaw_ = width > depth ? 90.0f : 0.0f;
        tile_length_ = std::max(width, depth);
        if (tile_length_ <= 0.001f) return false;

        const float center_x = (road_bounds_.minimum.x + road_bounds_.maximum.x) * 0.5f;
        const float center_z = (road_bounds_.minimum.z + road_bounds_.maximum.z) * 0.5f;
        const float radians = road_yaw_ * (pi / 180.0f);
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const float rotated_center_x = cosine * center_x + sine * center_z;
        const float rotated_center_z = -sine * center_x + cosine * center_z;

        road_local_ = Renderer::Transform{
            .position = {
                -rotated_center_x,
                -road_bounds_.minimum.y,
                -rotated_center_z,
            },
            .rotation = {0.0f, road_yaw_, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        };
        return true;
    }

    bool attachRoad(Ecs::Entity root)
    {
        bool attached = false;
        const std::size_t parts = Models::partCount(road_model_);
        for (std::size_t index = 0; index < parts; ++index) {
            const Models::ModelPart *part = Models::part(road_model_, index);
            if (!part || part->mesh == Models::INVALID_MESH) continue;

            const Ecs::Entity child = world_.createEntity();
            world_.add<Renderer::Transform>(child, road_local_);
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

    void recycleRoad()
    {
        const Renderer::Transform *camera_transform = world_.get<Renderer::Transform>(camera_);
        if (!camera_transform || tiles_.empty()) return;

        const long long camera_index = static_cast<long long>(
            std::floor(camera_transform->position.z / tile_length_)
        );
        bool changed = false;

        for (;;) {
            auto minimum = std::min_element(
                tiles_.begin(),
                tiles_.end(),
                [](const Tile& a, const Tile& b) { return a.index < b.index; }
            );
            auto maximum = std::max_element(
                tiles_.begin(),
                tiles_.end(),
                [](const Tile& a, const Tile& b) { return a.index < b.index; }
            );

            if (camera_index < minimum->index + tile_guard) {
                maximum->index = minimum->index - 1;
                Renderer::Transform *transform = world_.get<Renderer::Transform>(maximum->entity);
                if (transform)
                    transform->position.z = static_cast<float>(maximum->index) * tile_length_;
                changed = true;
                continue;
            }

            if (camera_index > maximum->index - tile_guard) {
                minimum->index = maximum->index + 1;
                Renderer::Transform *transform = world_.get<Renderer::Transform>(minimum->entity);
                if (transform)
                    transform->position.z = static_cast<float>(minimum->index) * tile_length_;
                changed = true;
                continue;
            }

            break;
        }

        if (changed) world_.markChanged(Ecs::ChangeKind::Transform);
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
    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;

    Ecs::World world_;
    Renderer::Rasterizer renderer_;
    Camera::FreeController camera_controller_;
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;

    Models::ModelHandle road_model_ = Models::INVALID_MODEL;
    Models::Bounds road_bounds_{};
    Renderer::Transform road_local_{};
    float road_yaw_ = 0.0f;
    float tile_length_ = 1.0f;
    std::string model_error_;
    std::vector<Tile> tiles_;
};

} // namespace

int main()
{
    Application application;
    return application.run();
}
