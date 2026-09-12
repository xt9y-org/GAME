#include "Dashcam/CameraMount.hpp"
#include "Dashcam/PostProcess.hpp"
#include "Sources/Camera.hpp"
#include "Sources/Ecs/Ecs.hpp"
#include "Sources/Models/Models.hpp"
#include "Sources/Models/Runtime.hpp"
#include "Sources/Renderer/Environment.hpp"
#include "Sources/Renderer/GlobalIllumination/Debug.hpp"
#include "Sources/Renderer/Math.hpp"
#include "Sources/Renderer/Render.hpp"
#include "Sources/Renderer/Scenes/SceneCache.hpp"
#include "Sources/UI/UI.hpp"

#include <imgui.h>
#include <lwcgl/context.h>
#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr long long tiles_ahead = 24;
constexpr long long road_loop_half_tiles = tiles_ahead;

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

            if (!game_frozen_ && !debug_camera_enabled_) {
                if (Renderer::Transform *camera_transform =
                        world_.get<Renderer::Transform>(camera_))
                {
                    dashcam_mount_.beginFrame(*camera_transform);
                }
            }

            updateDebugMode();
            if (ui_ready_) {
                const bool ui_visible = ::UI::beginFrame();
                if (debug_visible_ && ui_visible) {
                    drawDebug();
                    drawRenderingDebug();
                }
            }

            if (!debug_visible_ && (!ui_ready_ || !::UI::wantsKeyboard())) {
                if (debug_camera_enabled_) {
                    debug_camera_controller_.update(world_, delta_seconds);
                } else if (!game_frozen_) {
                    camera_controller_.update(world_, delta_seconds);
                }
            }

            if (!game_frozen_) {
                wrapRoadOrigin();
                if (!debug_camera_enabled_) {
                    if (Renderer::Transform *camera_transform =
                            world_.get<Renderer::Transform>(camera_))
                    {
                        dashcam_mount_.update(
                            *camera_transform,
                            delta_seconds,
                            dashcam_settings_,
                            dashcam_runtime_
                        );
                        world_.markChanged(Ecs::ChangeKind::Camera);
                    }
                } else {
                    dashcam_runtime_.delta_seconds = delta_seconds;
                    dashcam_runtime_.time_seconds += delta_seconds;
                    dashcam_runtime_.speed = 0.0f;
                    dashcam_runtime_.turn_rate = 0.0f;
                    dashcam_runtime_.acceleration = 0.0f;
                    dashcam_runtime_.g_force = 0.0f;
                }
            } else {
                dashcam_runtime_.delta_seconds = 0.0f;
            }

            resizeIfNeeded();
            renderer_manager_.render(world_);
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

        rasterizer_ = &renderer_manager_.add<Renderer::Rasterizer>("Rasterizer");
        rasterizer_->setEnabled(true);
        rasterizer_->setViewportCulling(false);
        rasterizer_->setShadowResolution(1024);
        rasterizer_->setFallbackShadowResolution(256);
        rasterizer_->setMinimumShadowResolution(64);
        rasterizer_->setShadowNearPlane(0.05f);
        rasterizer_->setShadowFarScale(1.05f);
        rasterizer_->setClearColor({0.38f, 0.50f, 0.66f, 1.0f});

        path_tracer_ = &renderer_manager_.add<Renderer::PathTracer>("PathTracer");
        path_tracer_->setEnabled(true);
        path_tracer_->setResolutionDivisor(2);
        path_tracer_->setSamplesPerFrame(1);
        path_tracer_->setStationaryPhaseGrid(1);
        path_tracer_->setResetPhaseGrid(1);
        path_tracer_->setMovingPhaseGrid(2);
        path_tracer_->setMovingDepthBlock(2);

        ray_tracer_ = &renderer_manager_.add<Renderer::RayTracer>("RayTracer");
        ray_tracer_->setEnabled(true);
        ray_tracer_->setResolutionDivisor(1);

        dashcam_pipeline_.add<Dashcam::LensPass>(dashcam_settings_, dashcam_runtime_);
        dashcam_pipeline_.add<Dashcam::SensorPass>(dashcam_settings_, dashcam_runtime_);
        dashcam_pipeline_.add<Dashcam::CompressionPass>(dashcam_settings_, dashcam_runtime_);
        dashcam_pipeline_.add<Dashcam::FrameHoldPass>(dashcam_settings_, dashcam_runtime_);
        renderer_manager_.setPostProcessPipeline(&dashcam_pipeline_);

        if (!renderer_manager_.initialize()) {
            std::fprintf(stderr, "failed to initialize a renderer\n");
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
        renderer_manager_.resize(framebuffer_width_, framebuffer_height_);
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

        const std::size_t streamed_tile_count = static_cast<std::size_t>(
            road_loop_half_tiles * 2 + 1
        );
        Renderer::Scenes::SceneCache::setMaximumTriangles(
            std::max<std::size_t>(tile_triangle_count_ * streamed_tile_count, 1u)
        );

        camera_ = world_.createEntity();
        world_.add<Renderer::Transform>(camera_, Renderer::Transform{
            .position = {0.0f, 3.0f, 0.0f},
            .rotation = {-12.0f, stack_on_x_ ? 90.0f : 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world_.add<Camera::CameraComponent>(camera_, Camera::CameraComponent{
            dashcam_settings_.fov_degrees, 0.05f, true
        });

        camera_controller_.setSpeed(16.0f);
        camera_controller_.setSprintMultiplier(10.0f);
        camera_controller_.setMouseSensitivity(0.10f);
        camera_controller_.setPitchRange(-89.0f, 89.0f);

        debug_camera_ = world_.createEntity();
        world_.add<Renderer::Transform>(debug_camera_, Renderer::Transform{
            .position = {0.0f, 3.0f, 0.0f},
            .rotation = {-12.0f, stack_on_x_ ? 90.0f : 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
        });
        world_.add<Camera::CameraComponent>(debug_camera_, Camera::CameraComponent{
            78.0f, 0.05f, false
        });

        debug_camera_controller_.setSpeed(24.0f);
        debug_camera_controller_.setSprintMultiplier(12.0f);
        debug_camera_controller_.setMouseSensitivity(0.10f);
        debug_camera_controller_.setPitchRange(-89.0f, 89.0f);

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

        global_illumination_ = world_.createEntity();
        world_.add<Renderer::GlobalIlluminationComponent>(
            global_illumination_,
            Renderer::GlobalIlluminationComponent{
                .enabled = false,
                .intensity = 1.0f,
                .bounces = 2u,
                .photon_mapping = false,
                .photon_count = 25000u,
                .photon_radius = 1.5f,
            }
        );

        if (!initializeRoadLoop()) {
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

    bool initializeRoadLoop()
    {
        for (long long index = -road_loop_half_tiles; index <= road_loop_half_tiles; ++index) {
            if (!createRoadTile(index)) return false;
        }
        return true;
    }

    void wrapRoadOrigin()
    {
        const Ecs::Entity active_camera = debug_camera_enabled_ ? debug_camera_ : camera_;
        Renderer::Transform *camera_transform = world_.get<Renderer::Transform>(active_camera);
        if (!camera_transform || tile_length_ <= 0.0f) return;

        float& camera_axis = stack_on_x_
            ? camera_transform->position.x
            : camera_transform->position.z;
        const float half_tile = tile_length_ * 0.5f;
        bool changed = false;

        while (camera_axis > half_tile) {
            camera_axis -= tile_length_;
            changed = true;
        }
        while (camera_axis < -half_tile) {
            camera_axis += tile_length_;
            changed = true;
        }

        if (changed) world_.markChanged(Ecs::ChangeKind::Camera);
    }

    const char *activeRendererName() const
    {
        const Renderer::Manager::Entry *entry = renderer_manager_.activeEntry();
        return entry ? entry->name.c_str() : "None";
    }

    void cycleRenderer()
    {
        if (!renderer_manager_.next())
            std::fprintf(stderr, "failed to switch renderer\n");
    }

    void applyDashcamFov()
    {
        Camera::CameraComponent *camera = world_.get<Camera::CameraComponent>(camera_);
        if (!camera || camera->fov_degrees == dashcam_settings_.fov_degrees) return;
        camera->fov_degrees = dashcam_settings_.fov_degrees;
        world_.markChanged(Ecs::ChangeKind::Camera);
    }

    void applyGiPauseState()
    {
        Renderer::GlobalIllumination::setPaused(game_frozen_ || gi_paused_);
    }

    void setGameFrozen(bool frozen)
    {
        if (game_frozen_ == frozen) return;
        game_frozen_ = frozen;
        applyGiPauseState();
    }

    void setDebugCameraEnabled(bool enabled)
    {
        if (debug_camera_enabled_ == enabled) return;

        Camera::CameraComponent *game_camera = world_.get<Camera::CameraComponent>(camera_);
        Camera::CameraComponent *debug_camera = world_.get<Camera::CameraComponent>(debug_camera_);
        if (!game_camera || !debug_camera) return;

        if (enabled) {
            const Renderer::Transform *game_transform = world_.get<Renderer::Transform>(camera_);
            Renderer::Transform *debug_transform = world_.get<Renderer::Transform>(debug_camera_);
            if (game_transform && debug_transform) *debug_transform = *game_transform;
        }

        game_camera->active = !enabled;
        debug_camera->active = enabled;
        debug_camera_enabled_ = enabled;
        world_.markChanged(Ecs::ChangeKind::Camera);
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

        const bool enter = Keyboard.isKeyDown(Keyboard.KEY_RETURN);
        if (enter && !enter_down_) cycleRenderer();
        enter_down_ = enter;
    }

    void drawDebug()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 430.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Driving Debug")) {
            ImGui::End();
            return;
        }

        if (ImGui::Button(game_frozen_ ? "Resume Game" : "Freeze Game"))
            setGameFrozen(!game_frozen_);
        ImGui::SameLine();
        ImGui::TextUnformatted(game_frozen_ ? "FROZEN" : "RUNNING");

        bool debug_camera_enabled = debug_camera_enabled_;
        if (ImGui::Checkbox("Debug Fly Camera", &debug_camera_enabled))
            setDebugCameraEnabled(debug_camera_enabled);
        if (debug_camera_enabled_)
            ImGui::TextUnformatted("Close debug with Tab to fly (WASD + mouse, Shift sprint)");

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

        ImGui::SeparatorText("Road Loop");
        ImGui::Text("Static tiles: %zu", road_tiles_.size());
        ImGui::Text("Visible reserve: %lld tiles each side", road_loop_half_tiles);
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
        ImGui::Text("Renderer: %s", activeRendererName());
        ImGui::TextUnformatted("Enter: next renderer | Tab: close debug");
        ImGui::End();
    }

    void drawRenderingDebug()
    {
        ImGui::SetNextWindowPos(ImVec2(364.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(390.0f, 650.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Rendering Debug")) {
            ImGui::End();
            return;
        }

        ImGui::SeparatorText("Renderer");
        ImGui::Text("Active: %s", activeRendererName());
        if (ImGui::Button("Next Renderer")) cycleRenderer();

        for (std::size_t index = 0; index < renderer_manager_.count(); ++index) {
            const Renderer::Manager::Entry *entry = renderer_manager_.entry(index);
            if (!entry) continue;
            ImGui::SameLine();
            const std::string label = entry->name + (entry->available ? "##renderer" : " (unavailable)##renderer");
            if (ImGui::Button(label.c_str()) && entry->available)
                renderer_manager_.activate(index);
        }

        if (rasterizer_ && ImGui::CollapsingHeader("Rasterizer")) {
            bool viewport_culling = rasterizer_->viewportCulling();
            if (ImGui::Checkbox("Viewport Culling", &viewport_culling))
                rasterizer_->setViewportCulling(viewport_culling);

            int shadow_resolution = rasterizer_->shadowResolution();
            if (ImGui::SliderInt("Shadow Resolution", &shadow_resolution, 64, 4096))
                rasterizer_->setShadowResolution(shadow_resolution);

            float clear_color[4] {
                rasterizer_->clearColor().x,
                rasterizer_->clearColor().y,
                rasterizer_->clearColor().z,
                rasterizer_->clearColor().w,
            };
            if (ImGui::ColorEdit4("Clear Color", clear_color)) {
                rasterizer_->setClearColor({
                    clear_color[0], clear_color[1], clear_color[2], clear_color[3]
                });
            }
        }

        if (path_tracer_ && ImGui::CollapsingHeader("Path Tracer")) {
            int resolution_divisor = path_tracer_->resolutionDivisor();
            if (ImGui::SliderInt("PT Resolution Divisor", &resolution_divisor, 1, 4)) {
                path_tracer_->setResolutionDivisor(resolution_divisor);
                path_tracer_->resize(framebuffer_width_, framebuffer_height_);
            }

            int samples = path_tracer_->samplesPerFrame();
            if (ImGui::SliderInt("Samples / Frame", &samples, 1, 16))
                path_tracer_->setSamplesPerFrame(samples);

            int stationary_grid = path_tracer_->stationaryPhaseGrid();
            if (ImGui::SliderInt("Stationary Phase Grid", &stationary_grid, 1, 8))
                path_tracer_->setStationaryPhaseGrid(stationary_grid);

            int moving_grid = path_tracer_->movingPhaseGrid();
            if (ImGui::SliderInt("Moving Phase Grid", &moving_grid, 1, 8))
                path_tracer_->setMovingPhaseGrid(moving_grid);
        }

        if (ray_tracer_ && ImGui::CollapsingHeader("Ray Tracer")) {
            int resolution_divisor = ray_tracer_->resolutionDivisor();
            if (ImGui::SliderInt("RT Resolution Divisor", &resolution_divisor, 1, 4)) {
                ray_tracer_->setResolutionDivisor(resolution_divisor);
                ray_tracer_->resize(framebuffer_width_, framebuffer_height_);
            }
        }

        ImGui::SeparatorText("Global Illumination");
        Renderer::GlobalIlluminationComponent *gi =
            world_.get<Renderer::GlobalIlluminationComponent>(global_illumination_);
        if (gi) {
            bool gi_changed = false;
            gi_changed |= ImGui::Checkbox("Enabled##gi", &gi->enabled);
            gi_changed |= ImGui::SliderFloat("GI Intensity", &gi->intensity, 0.0f, 4.0f);

            int bounces = static_cast<int>(gi->bounces);
            if (ImGui::SliderInt("Bounces", &bounces, 1, 8)) {
                gi->bounces = static_cast<std::uint8_t>(bounces);
                gi_changed = true;
            }

            gi_changed |= ImGui::Checkbox("Photon Mapping", &gi->photon_mapping);
            int photon_count = static_cast<int>(gi->photon_count);
            if (ImGui::SliderInt("Photon Count", &photon_count, 1000, 200000)) {
                gi->photon_count = static_cast<std::uint32_t>(photon_count);
                gi_changed = true;
            }
            gi_changed |= ImGui::SliderFloat("Photon Radius", &gi->photon_radius, 0.05f, 10.0f);

            bool gi_paused = gi_paused_;
            if (ImGui::Checkbox("Pause GI", &gi_paused)) {
                gi_paused_ = gi_paused;
                applyGiPauseState();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset GI")) Renderer::GlobalIllumination::reset();

            if (gi_changed) world_.markChanged();

            const Renderer::GlobalIllumination::Debug::Statistics stats =
                Renderer::GlobalIllumination::Debug::statistics();
            ImGui::Text(
                "Probes: %zu | Photons: %zu | GI: %.0f%%",
                stats.probes,
                stats.photons,
                stats.progress * 100.0f
            );
            ImGui::Text(
                "Triangles: %zu | BVH nodes: %zu | depth: %u",
                stats.triangles,
                stats.bvh_nodes,
                stats.bvh_depth
            );
        }

        ImGui::SeparatorText("Dashcam Post Processing");
        ImGui::Checkbox("Enabled##dashcam", &dashcam_settings_.enabled);
        ImGui::SameLine();
        if (ImGui::Button("Reset Dashcam")) {
            Dashcam::reset(dashcam_settings_);
            applyDashcamFov();
        }

        if (ImGui::SliderFloat("FOV", &dashcam_settings_.fov_degrees, 90.0f, 120.0f, "%.0f deg"))
            applyDashcamFov();
        ImGui::SliderFloat("Capture FPS", &dashcam_settings_.capture_fps, 8.0f, 60.0f, "%.0f");
        ImGui::SliderFloat(
            "Virtual Height",
            &dashcam_settings_.virtual_height,
            240.0f,
            1080.0f,
            "%.0f px"
        );

        if (ImGui::CollapsingHeader("Lens / Glass", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Barrel Distortion", &dashcam_settings_.barrel_distortion, 0.0f, 0.40f);
            ImGui::SliderFloat(
                "Chromatic Aberration",
                &dashcam_settings_.chromatic_aberration,
                0.0f,
                0.015f,
                "%.4f"
            );
            ImGui::SliderFloat("Vignette", &dashcam_settings_.vignette, 0.0f, 0.90f);
            ImGui::SliderFloat("Rolling Shutter", &dashcam_settings_.rolling_shutter, 0.0f, 0.08f);
            ImGui::SliderFloat("Dirt / Smudges", &dashcam_settings_.dirt, 0.0f, 0.80f);
            ImGui::SliderFloat(
                "Windshield Reflection",
                &dashcam_settings_.windshield_reflection,
                0.0f,
                0.50f
            );
        }

        if (ImGui::CollapsingHeader("Sensor / Color", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Exposure", &dashcam_settings_.exposure, 0.10f, 4.0f);
            ImGui::SliderFloat("Shadow Crush", &dashcam_settings_.shadow_crush, 0.0f, 0.50f);
            ImGui::SliderFloat("Highlight Clip", &dashcam_settings_.highlight_clip, 0.20f, 4.0f);
            ImGui::SliderFloat("Desaturation", &dashcam_settings_.desaturation, 0.0f, 1.0f);
            ImGui::SliderFloat("Green Tint", &dashcam_settings_.green_tint, -0.15f, 0.15f);
            ImGui::SliderFloat("Yellow Tint", &dashcam_settings_.yellow_tint, -0.15f, 0.15f);
            ImGui::SliderFloat("ISO / Digital Noise", &dashcam_settings_.noise, 0.0f, 0.20f);
            ImGui::SliderFloat("Color Bleed", &dashcam_settings_.color_bleed, 0.0f, 0.80f);
        }

        if (ImGui::CollapsingHeader("Compression / Motion", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Macroblocking", &dashcam_settings_.macroblocking, 0.0f, 1.0f);
            ImGui::SliderFloat("Interlacing", &dashcam_settings_.interlacing, 0.0f, 0.35f);
            ImGui::SliderFloat("G-force Glitch", &dashcam_settings_.glitch, 0.0f, 1.0f);
            ImGui::SliderFloat("Mount Vibration", &dashcam_settings_.vibration, 0.0f, 1.0f);
            ImGui::SliderFloat("Mount Inertia", &dashcam_settings_.inertia, 0.0f, 1.0f);
        }

        ImGui::Text(
            "Dashcam: %.1f m/s | turn %.1f deg/s | %.2f g",
            dashcam_runtime_.speed,
            dashcam_runtime_.turn_rate,
            dashcam_runtime_.g_force
        );

        ImGui::End();
    }

    void resizeIfNeeded()
    {
        const int width = std::max(Display.getWidth(), 1);
        const int height = std::max(Display.getHeight(), 1);
        if (width == framebuffer_width_ && height == framebuffer_height_) return;

        framebuffer_width_ = width;
        framebuffer_height_ = height;
        renderer_manager_.resize(width, height);
    }

    void shutdown()
    {
        if (!started_) return;
        if (ui_ready_) ::UI::shutdown();
        renderer_manager_.shutdown();
        Renderer::GlobalIllumination::reset();
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
    bool enter_down_ = false;
    bool game_frozen_ = false;
    bool debug_camera_enabled_ = false;
    bool gi_paused_ = false;
    bool stack_on_x_ = false;
    int framebuffer_width_ = 1;
    int framebuffer_height_ = 1;

    Ecs::World world_;
    Renderer::Manager renderer_manager_;
    Renderer::PostProcess::Pipeline dashcam_pipeline_;
    Renderer::Rasterizer *rasterizer_ = nullptr;
    Renderer::PathTracer *path_tracer_ = nullptr;
    Renderer::RayTracer *ray_tracer_ = nullptr;
    Camera::FreeController camera_controller_;
    Camera::FreeController debug_camera_controller_;
    Dashcam::Settings dashcam_settings_{};
    Dashcam::Runtime dashcam_runtime_{};
    Dashcam::CameraMount dashcam_mount_;
    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    Ecs::Entity debug_camera_ = Ecs::INVALID_ENTITY;
    Ecs::Entity environment_ = Ecs::INVALID_ENTITY;
    Ecs::Entity sun_ = Ecs::INVALID_ENTITY;
    Ecs::Entity global_illumination_ = Ecs::INVALID_ENTITY;

    Models::ModelHandle road_model_ = Models::INVALID_MODEL;
    Models::Runtime::Pose road_pose_{};
    Models::Bounds road_bounds_{};
    Renderer::Vec3 model_offset_{};
    std::map<long long, RoadTile> road_tiles_;
    std::size_t tile_triangle_count_ = 0u;
    float tile_length_ = 1.0f;
    float frame_seconds_ = 0.0f;
    std::string model_error_;
};

} // namespace

int main()
{
    Application application;
    return application.run();
}
