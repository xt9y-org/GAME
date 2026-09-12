#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Renderer/Components.hpp"
#include "Renderer/Hierarchy.hpp"

namespace Tests {
namespace {

class HierarchyCase final : public Testing::Case {
public:
    std::string_view name() const override { return "rendering/hierarchy"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity parent = world.createEntity();
        world.add<Renderer::Transform>(parent, Renderer::Transform{.position = {1.0f, 2.0f, 3.0f}});
        const Ecs::Entity child = world.createEntity();
        world.add<Renderer::Transform>(child, Renderer::Transform{.position = {4.0f, 5.0f, 6.0f}});

        if (!Testing::require(Renderer::Hierarchy::setParent(world, child, parent), "setParent failed", error)) return false;
        Renderer::Transform transform;
        if (!Testing::require(Renderer::Hierarchy::worldTransform(world, child, &transform), "worldTransform failed", error)) return false;
        if (!Testing::require(
                Testing::near(transform.position.x, 5.0f) && Testing::near(transform.position.y, 7.0f) &&
                Testing::near(transform.position.z, 9.0f), "hierarchy world position mismatch", error)) return false;
        if (!Testing::require(!Renderer::Hierarchy::setParent(world, parent, child), "hierarchy cycle accepted", error)) return false;
        if (!Testing::require(Renderer::Hierarchy::clearParent(world, child), "clearParent failed", error)) return false;
        return Testing::require(world.get<Renderer::Parent>(child) == nullptr, "parent survived clearParent", error);
    }
};

} // namespace

void registerHierarchy(Testing::Runner& runner)
{
    runner.add<HierarchyCase>();
}

} // namespace Tests
