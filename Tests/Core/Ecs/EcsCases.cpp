#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Ecs/Ecs.hpp"

namespace Tests {
namespace {

struct Position { int value = 0; };
struct Velocity { int value = 0; };

class EcsCase final : public Testing::Case {
public:
    std::string_view name() const override { return "core/ecs"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity a = world.createEntity();
        const Ecs::Entity b = world.createEntity();
        world.add<Position>(a, Position{2});
        world.add<Position>(b, Position{4});
        world.add<Velocity>(b, Velocity{3});

        int sum = 0;
        world.each<Position, Velocity>([&](Ecs::Entity entity, Position& p, Velocity& v) {
            if (entity == b) sum += p.value + v.value;
        });
        return Testing::require(world.size() == 2u, "entity count mismatch", error) &&
            Testing::require(world.has<Position>(a), "component missing", error) &&
            Testing::require(world.get<Velocity>(a) == nullptr, "unexpected component", error) &&
            Testing::require(sum == 7, "multi-component iteration mismatch", error) &&
            Testing::require(world.remove<Position>(a), "component removal failed", error) &&
            Testing::require(!world.has<Position>(a), "removed component still present", error);
    }
};

class LifecycleCase final : public Testing::Case {
public:
    std::string_view name() const override { return "core/lifecycle"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = world.createEntity();
        world.add<Position>(entity, Position{9});
        if (!Testing::require(world.alive(entity), "new entity is not alive", error)) return false;
        if (!Testing::require(world.destroyEntity(entity), "destroy failed", error)) return false;
        return Testing::require(!world.alive(entity), "destroyed entity is alive", error) &&
            Testing::require(world.get<Position>(entity) == nullptr, "destroy did not remove components", error) &&
            Testing::require(!world.destroyEntity(entity), "double destroy unexpectedly succeeded", error);
    }
};

class RevisionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "core/change-revisions"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const std::uint64_t initial = world.changeRevision();
        const Ecs::Entity entity = world.createEntity();
        const std::uint64_t after_create = world.changeRevision();
        world.markChanged(Ecs::ChangeKind::Transform);
        const std::uint64_t transform = world.changeRevision(Ecs::ChangeKind::Transform);
        world.add<Position>(entity, Position{});
        return Testing::require(after_create > initial, "create did not change revision", error) &&
            Testing::require(transform > 0u, "transform revision did not advance", error) &&
            Testing::require(world.changeRevision(Ecs::ChangeKind::Structure) > 0u, "structure revision did not advance", error);
    }
};

} // namespace

void registerCore(Testing::Runner& runner)
{
    runner.add<EcsCase>();
    runner.add<LifecycleCase>();
    runner.add<RevisionCase>();
}

} // namespace Tests
