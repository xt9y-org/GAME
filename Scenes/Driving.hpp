#ifndef GAME_SCENES_DRIVING_HPP
#define GAME_SCENES_DRIVING_HPP

#include "Driving/Traffic.hpp"
#include "Scenes/Scene.hpp"

#include "Sources/Models/Models.hpp"
#include "Sources/Renderer/Components.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Game::Scenes {

class Driving final : public Scene {
public:
    const char *name() const override;
    bool load(Ecs::World& world, std::string& error) override;
    void update(Ecs::World& world, float delta_seconds) override;
    Ecs::Entity camera() const override;
    std::size_t triangleCount() const override;
    bool usesFreeCamera() const override { return false; }
    bool showStats() const override { return false; }
    void drawDebug(Ecs::World& world) override;
    void emitMetrics(const std::function<void(std::string_view, double)>& emit) const override;

private:
    void createRoad(Ecs::World& world);
    void createTraffic(Ecs::World& world);
    void recycleRoad(Ecs::World& world);
    void addRenderable(
        Ecs::World& world,
        Models::MeshHandle mesh,
        Models::MaterialHandle material,
        const Renderer::Transform& transform,
        bool road_piece = false
    );

    float lane_width_ = 3.65f;
    int lane_count_ = 4;
    float segment_length_ = 120.0f;
    int segment_count_ = 28;
    float road_width_ = 18.6f;
    float traffic_spawn_ahead_ = 700.0f;
    float traffic_despawn_behind_ = 140.0f;
    int traffic_count_ = 34;

    Ecs::Entity camera_ = Ecs::INVALID_ENTITY;
    Ecs::Entity player_ = Ecs::INVALID_ENTITY;
    std::size_t triangle_count_ = 0u;
    Game::Driving::TrafficSystem traffic_system_{};

    Models::MeshHandle road_mesh_ = Models::INVALID_MESH;
    Models::MeshHandle marking_mesh_ = Models::INVALID_MESH;
    Models::MeshHandle box_mesh_ = Models::INVALID_MESH;
    Models::MaterialHandle asphalt_ = Models::INVALID_MATERIAL;
    Models::MaterialHandle markings_ = Models::INVALID_MATERIAL;
    Models::MaterialHandle concrete_ = Models::INVALID_MATERIAL;
    std::vector<Models::MaterialHandle> traffic_materials_;
};

} // namespace Game::Scenes

#endif
