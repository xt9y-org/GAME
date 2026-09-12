#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Physics/Physics.hpp"
#include "Renderer/Components.hpp"

#include <algorithm>

namespace Tests {
namespace {

struct PhysicsScene {
    Ecs::World world;
    Ecs::Entity box = Ecs::INVALID_ENTITY;
    Ecs::Entity sphere = Ecs::INVALID_ENTITY;
    Ecs::Entity capsule = Ecs::INVALID_ENTITY;

    PhysicsScene()
    {
        box = world.createEntity();
        world.add<Renderer::Transform>(box, Renderer::Transform{.position = {0.0f, 0.0f, 5.0f}});
        world.add<Physics::BoxCollider>(box, Physics::BoxCollider{});

        sphere = world.createEntity();
        world.add<Renderer::Transform>(sphere, Renderer::Transform{.position = {3.0f, 0.0f, 5.0f}});
        world.add<Physics::SphereCollider>(sphere, Physics::SphereCollider{.radius = 0.75f});

        capsule = world.createEntity();
        world.add<Renderer::Transform>(capsule, Renderer::Transform{.position = {-3.0f, 0.0f, 5.0f}});
        world.add<Physics::CapsuleCollider>(capsule, Physics::CapsuleCollider{.radius = 0.5f, .half_height = 1.0f});
    }
};

class ColliderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "physics/colliders"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        PhysicsScene scene;
        const Physics::SpatialStats stats = Physics::spatialStats(scene.world);
        return Testing::require(stats.colliders == 3u, "collider count mismatch", error) &&
            Testing::require(stats.nodes > 0u, "spatial index did not build", error) &&
            Testing::require(stats.rebuilds > 0u, "spatial index rebuild not recorded", error);
    }
};

class RaycastCase final : public Testing::Case {
public:
    std::string_view name() const override { return "physics/raycast"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        PhysicsScene scene;
        Physics::RaycastHit hit;
        const bool found = Physics::raycast(scene.world, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 100.0f, &hit);
        return Testing::require(found, "raycast missed box", error) &&
            Testing::require(hit.entity == scene.box, "raycast returned wrong entity", error) &&
            Testing::require(Testing::near(hit.distance, 4.5f, 1.0e-3f), "raycast distance mismatch", error) &&
            Testing::require(hit.normal.z < -0.99f, "raycast normal mismatch", error);
    }
};

class SphereCastCase final : public Testing::Case {
public:
    std::string_view name() const override { return "physics/sphere-cast"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        PhysicsScene scene;
        Physics::RaycastHit hit;
        const bool found = Physics::sphereCast(scene.world, {0.0f, 0.0f, 0.0f}, 0.5f, {0.0f, 0.0f, 1.0f}, 100.0f, &hit);
        return Testing::require(found && hit.entity == scene.box, "sphere cast hit mismatch", error) &&
            Testing::require(Testing::near(hit.distance, 4.0f, 1.0e-3f), "sphere cast distance mismatch", error);
    }
};

class OverlapSphereCase final : public Testing::Case {
public:
    std::string_view name() const override { return "physics/overlap-sphere"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        PhysicsScene scene;
        std::vector<Ecs::Entity> hits;
        Physics::overlapSphere(scene.world, {3.0f, 0.0f, 5.0f}, 0.1f, hits);
        return Testing::require(std::find(hits.begin(), hits.end(), scene.sphere) != hits.end(),
                                "overlap sphere did not return sphere collider", error) &&
            Testing::require(std::find(hits.begin(), hits.end(), scene.box) == hits.end(),
                             "overlap sphere returned distant box", error);
    }
};

class OverlapAabbCase final : public Testing::Case {
public:
    std::string_view name() const override { return "physics/overlap-aabb"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        PhysicsScene scene;
        std::vector<Ecs::Entity> hits;
        Physics::overlapAabb(scene.world, {-3.75f, -2.0f, 4.0f}, {-2.25f, 2.0f, 6.0f}, hits);
        return Testing::require(std::find(hits.begin(), hits.end(), scene.capsule) != hits.end(),
                                "overlap AABB did not return capsule", error) &&
            Testing::require(std::find(hits.begin(), hits.end(), scene.box) == hits.end(),
                             "overlap AABB returned distant box", error);
    }
};

} // namespace

void registerPhysics(Testing::Runner& runner)
{
    runner.add<ColliderCase>();
    runner.add<RaycastCase>();
    runner.add<SphereCastCase>();
    runner.add<OverlapSphereCase>();
    runner.add<OverlapAabbCase>();
}

} // namespace Tests
