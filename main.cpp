#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Models/Runtime.hpp"
#include "Sources/Renderer/Environment.hpp"
#include "Sources/Renderer/Math.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr long long tiles_ahead = 24;
constexpr long long tiles_behind = 8;

struct RoadTile {
    Ecs::Entity root = Ecs::INVALID_ENTITY;
    std::vector<Ecs::Entity> children;
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
            frame_seconds_ = delta_seconds;

            updateDebugMode();
            if (ui_ready_) {
                const bool ui_visible = ::UI::beginFrame();
                if (debug_visible_ && ui_visible) drawDebug();
            }

            if (!debug_visible_ && (!ui_ready_ || !::UI::wantsKeyboard()))
                camera_controller_.update(world_, delta_seconds);

            if (!road_stream_failed_ && !updateRoadWindow()) {
                road_stream_failed_ = true;
                std::fprintf(stderr, "failed to stream infinite road\n");
            }

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

        ui_ready_ = ::UI::init();
        if (ui_ready_) {
            ImGui::StyleColorsDark();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
        } else {
            std::fprintf(stderr, "debug UI unavailable\n");
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

        environment_ = world_.createEntity();
        world_.add<Renderer::EnvironmentComponent>(
            environment_,
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

        sun_ = world_.createEntity();
        world_.add<Renderer::Transform>(sun_, Renderer::Transform{
            .position = {},
            .rotation = {-50.0f, -35.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world_.add<Renderer::LightComponent>(sun_, Renderer::LightComponent{
            .type = Renderer::LightType::Directional,
            .color = {1.0f, 0.96f, 0.88f},
            .intensity = 1.0f,
            .range = 0.0f,
        });

        if (!updateRoadWindow()) {
            std::fprintf(stderr, "road_tile.glb contains no renderable parts\n");
            return false;
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
        tile_triangle_count_ = 0u;

        bool found = false;
        const std::size_t parts = Models::partCount(road_model_);
        for (std::size_t part_index = 0; part_index < parts; ++part_index) {
            const Models::ModelPart *part = Models::part(road_model_, part_index);
            if (!part) continue;

            const Models::MeshData *mesh = Models::mesh(part->mesh);
            if (!mesh || mesh->vertices.empty()) continue;

            if (mesh->primitive_mode == Models::PrimitiveMode::Triangles) {
                tile_triangle_count_ += mesh->indices.empty()
                    ? mesh->vertices.size() / 3u
                    : mesh->indices.size() / 3u;
            }

            const Models::Mat4 matrix = partMatrix(*part);
            for (const Models::Vertex& vertex : mesh->vertices) {
                const Renderer::Vec3 point = Renderer::Math::transformPoint(
                    matrix,
                    {
                        vertex.position.x,
                        vertex.position.y,
                        vertex.position.z,
                    }
                );

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

        // Keep the working authored-road orientation and visual long-axis mapping.
        stack_on_x_ = depth > width;
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
        if (road_tiles_.find(index) != road_tiles_.end()) return true;

        RoadTile tile;
        tile.root = world_.createEntity();
        world_.add<Renderer::Transform>(tile.root, Renderer::Transform{
            .position = tilePosition(index),
            .rotation = {},
            .scale = {1.0f, 1.0f, 1.0f},
        });

        const std::size_t parts = Models::partCount(road_model_);
        for (std::size_t part_index = 0; part_index < parts; ++part_index) {
            const Models::ModelPart *part = Models::part(road_model_, part_index);
            if (!part || part->mesh == Models::INVALID_MESH) continue;

            Renderer::Transform authored_transform{};
            authored_transform.matrix_override = partMatrix(*part);
            authored_transform.matrix_override_enabled = true;

            const Ecs::Entity child = world_.createEntity();
            world_.add<Renderer::Transform>(child, authored_transform);
            world_.add<Renderer::Parent>(child, Renderer::Parent{tile.root});
            world_.add<Renderer::MeshComponent>(
                child,
                Renderer::MeshComponent{part->mesh, part->material}
            );
            world_.add<Renderer::RenderableComponent>(
                child,
                Renderer::RenderableComponent{true}
            );
            tile.children.push_back(child);
        }

        if (tile.children.empty()) {
            world_.destroyEntity(tile.root);
            return false;
        }

        road_tiles_.emplace(index, std::move(tile));
        return true;
    }

    void destroyRoadTile(long long index)
    {
        const auto found = road_tiles_.find(index);
        if (found == road_tiles_.end()) return;

        RoadTile& tile = found->second;
        for (auto child = tile.children.rbegin(); child != tile.children.rend(); ++child)
            world_.destroyEntity(*child);
        world_.destroyEntity(tile.root);
        road_tiles_.erase(found);
    }

    bool updateRoadWindow()
    {
        const Renderer::Transform *camera_transform = world_.get<Renderer::Transform>(camera_);
        if (!camera_transform) return false;

        const float camera_axis = stack_on_x_
            ? camera_transform->position.x
            : camera_transform->position.z;

        if (have_last_camera_axis_) {
            const float movement = camera_axis - last_camera_axis_;
            if (std::abs(movement) > 0.05f)
                travel_direction_ = movement > 0.0f ? 1 : -1;
        }
        last_camera_axis_ = camera_axis;
        have_last_camera_axis_ = true;

        const long long camera_index = static_cast<long long>(
            std::floor(camera_axis / tile_length_)
        );

        const long long minimum_index = travel_direction_ < 0
            ? camera_index - tiles_ahead
            : camera_index - tiles_behind;
        const long long maximum_index = travel_direction_ < 0
            ? camera_index + tiles_behind
            : camera_index + tiles_ahead;

        for (long long index = minimum_index; index <= maximum_index; ++index) {
            if (!createRoadTile(index)) return false;
        }

        std::vector<long long> stale;
        stale.reserve(road_tiles_.size());
        for (const auto& entry : road_tiles_) {
            if (entry.first < minimum_index || entry.first > maximum_index)
                stale.push_back(entry.first);
        }
        for (const long long index : stale) destroyRoadTile(index);

        return true;
    }

    void updateDebugMode()
    {
        const bool tab = Keyboard.isKeyDown(Keyboard.KEY_TAB);
        if (tab && !tab_down_) {
            debug_visible_ = !debug_visible_;
            Mouse.setGrabbed(debug_visible_ ? LWCGL_FALSE : LWCGL_TRUE);
            Mouse.getDX();
            Mouse.getDY();
        }
        tab_down_ = tab;
    }

    void drawDebug()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 430.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Driving Debug")) {
            ImGui::End();
            return;
        }

        bool lighting_changed = false;
        Renderer::EnvironmentComponent *environment =
            world_.get<Renderer::EnvironmentComponent>(environment_);
        Renderer::LightComponent *light = world_.get<Renderer::LightComponent>(sun_);
        Renderer::Transform *light_transform = world_.get<Renderer::Transform>(sun_);

        ImGui::SeparatorText("Lighting");
        if (environment) {
            lighting_changed |= ImGui::SliderFloat(
                "Ambient Intensity",
                &environment->ambient_intensity,
                0.0f,
                3.0f
            );
            lighting_changed |= ImGui::SliderFloat(
                "Sky Intensity",
                &environment->intensity,
                0.0f,
                3.0f
            );

            float sky_color[3] {
                environment->sky_color.x,
                environment->sky_color.y,
                environment->sky_color.z,
            };
            if (ImGui::ColorEdit3("Sky Color", sky_color)) {
                environment->sky_color = {sky_color[0], sky_color[1], sky_color[2]};
                lighting_changed = true;
            }

            float ambient_color[3] {
                environment->ambient_color.x,
                environment->ambient_color.y,
                environment->ambient_color.z,
            };
            if (ImGui::ColorEdit3("Ambient Color", ambient_color)) {
                environment->ambient_color = {
                    ambient_color[0],
                    ambient_color[1],
                    ambient_color[2],
                };
                lighting_changed = true;
            }
        }

        if (light) {
            lighting_changed |= ImGui::SliderFloat(
                "Sun Intensity",
                &light->intensity,
                0.0f,
                10.0f
            );

            float sun_color[3] {light->color.x, light->color.y, light->color.z};
            if (ImGui::ColorEdit3("Sun Color", sun_color)) {
                light->color = {sun_color[0], sun_color[1], sun_color[2]};
                lighting_changed = true;
            }
        }

        if (light_transform) {
            lighting_changed |= ImGui::SliderFloat(
                "Sun Pitch",
                &light_transform->rotation.x,
                -89.0f,
                89.0f
            );
            lighting_changed |= ImGui::SliderFloat(
                "Sun Yaw",
                &light_transform->rotation.y,
                -180.0f,
                180.0f
            );
        }

        if (lighting_changed) world_.markChanged();

        ImGui::SeparatorText("Streaming");
        ImGui::Text("Tiles: %zu", road_tiles_.size());
        ImGui::Text("Range: %lld ahead / %lld behind", tiles_ahead, tiles_behind);
        ImGui::Text("Axis: %s", stack_on_x_ ? "X" : "Z");
        if (!road_tiles_.empty()) {
            ImGui::Text(
                "Indices: %lld .. %lld",
                road_tiles_.begin()->first,
                road_tiles_.rbegin()->first
            );
        }

        ImGui::SeparatorText("Frame");
        const float fps = frame_seconds_ > 0.0f ? 1.0f / frame_seconds_ : 0.0f;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("World entities: %zu", world_.size());
        ImGui::Text(
            "Road triangles: %zu",
            tile_triangle_count_ * road_tiles_.size()
        );

        ImGui::Separator();
        ImGui::TextUnformatted("Tab: close debug");
        ImGui::End();
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
        if (ui_ready_) ::UI::shutdown();
        renderer_.shutdown();
        Models::clearCache();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        started_ = false;
    }

    bool started_ = false;
    bool ui_ready_ = false;
    bool debug_visible_ = false;
    bool tab_down_ = false;
    bool stack_on_x_ = false;
    bool road_stream_failed_ = false;
    bool have_last_camera_axis_ = false;
    int travel_direction_ = -1;
    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;

    Ecs::World world_;
    Renderer::Rasterizer renderer_;
    Camera::FreeController camera_controller_;
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    Ecs::Entity environment_ = Ecs::INVALID_ENTITY;
    Ecs::Entity sun_ = Ecs::INVALID_ENTITY;

    Models::ModelHandle road_model_ = Models::INVALID_MODEL;
    Models::Runtime::Pose road_pose_{};
    Models::Bounds road_bounds_{};
    Renderer::Vec3 model_offset_{};
    std::map<long long, RoadTile> road_tiles_;
    std::size_t tile_triangle_count_ = 0u;
    float tile_length_ = 1.0f;
    float last_camera_axis_ = 0.0f;
    float frame_seconds_ = 0.0f;
    std::string model_error_;
};

} // namespace

int main()
{
    Application application;
    return application.run();
}
