#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Interactivity/Interactivity.hpp"
#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/ModelScene.hpp"

#include <array>

namespace Tests {
namespace {

struct RuntimeFixture {
    Ecs::World world;
    Renderer::ModelScene::Instance instance;
    Interactivity::Runtime runtime;
    std::string error;

    bool load(const char *path = "Assets/Interactivity/basic.gltf")
    {
        Models::clearCache();
        const Models::ModelHandle model = Models::load(path, &error);
        if (model == Models::INVALID_MODEL) return false;
        if (!Renderer::ModelScene::instantiate(world, model, &instance, {}, &error)) return false;
        return runtime.load(world, instance, &error);
    }
};

class RuntimeCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/runtime"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load()) { error = fixture.error; return false; }
        std::vector<double> answer;
        return Testing::require(fixture.runtime.active(), "runtime did not activate", error) &&
            Testing::require(fixture.runtime.graphName() == "GAME regression", "graph name mismatch", error) &&
            Testing::require(fixture.runtime.variable("answer", &answer) && answer.size() == 1u && Testing::near(answer[0], 42.0),
                             "onStart/math/variable flow mismatch", error);
    }
};

class EventCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/events"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load()) { error = fixture.error; return false; }
        const std::array<Interactivity::EventValue, 1> values{{{"value", {7.0}}}};
        if (!fixture.runtime.send(fixture.world, "set-event", values, &fixture.error)) {
            error = fixture.error;
            return false;
        }
        std::vector<double> event_value;
        return Testing::require(fixture.runtime.variable("eventValue", &event_value) && event_value.size() == 1u &&
                                Testing::near(event_value[0], 7.0), "event receive/value propagation mismatch", error) &&
            Testing::require(fixture.runtime.statistics().events > 0u, "event statistics did not advance", error);
    }
};

class PointerCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/pointers"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load()) { error = fixture.error; return false; }
        if (!Testing::require(!fixture.instance.nodes.empty(), "model scene has no node bindings", error)) return false;
        const Ecs::Entity entity = fixture.instance.nodes.front().entity;
        const Renderer::Transform *transform = fixture.world.get<Renderer::Transform>(entity);
        return Testing::require(transform != nullptr, "pointer target transform missing", error) &&
            Testing::require(Testing::near(transform->position.x, 1.0f) && Testing::near(transform->position.y, 2.0f) &&
                             Testing::near(transform->position.z, 3.0f), "pointer/set did not update model scene transform", error) &&
            Testing::require(fixture.runtime.statistics().pointer_writes > 0u, "pointer statistics did not advance", error);
    }
};

class SequenceOrderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/sequence-order"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/sequence-order.gltf")) { error = fixture.error; return false; }
        if (!Testing::require(!fixture.instance.nodes.empty(), "sequence-order scene has no node bindings", error)) return false;
        const Ecs::Entity entity = fixture.instance.nodes.front().entity;
        const Renderer::Transform *transform = fixture.world.get<Renderer::Transform>(entity);
        return Testing::require(transform != nullptr, "sequence-order pointer target missing", error) &&
            Testing::require(Testing::near(transform->position.x, 9.0f) && Testing::near(transform->position.y, 0.0f) &&
                             Testing::near(transform->position.z, 0.0f),
                             "flow/sequence sockets were not executed in lexicographic order", error);
    }
};

class MatrixCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/matrix-operations"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/matrix.gltf")) { error = fixture.error; return false; }

        std::vector<double> det2;
        std::vector<double> det3;
        std::vector<double> combine2;
        std::vector<double> inverse2;
        return Testing::require(fixture.runtime.variable("det2", &det2) && det2.size() == 1u && Testing::near(det2[0], -2.0),
                                "float2x2 determinant mismatch", error) &&
            Testing::require(fixture.runtime.variable("det3", &det3) && det3.size() == 1u && Testing::near(det3[0], 24.0),
                             "float3x3 determinant mismatch", error) &&
            Testing::require(fixture.runtime.variable("combine2", &combine2) && combine2.size() == 1u && Testing::near(combine2[0], 3.0),
                             "combine2x2/extract2x2 socket mismatch", error) &&
            Testing::require(fixture.runtime.variable("inverse2", &inverse2) && inverse2.size() == 1u && Testing::near(inverse2[0], 0.6, 1.0e-5),
                             "float2x2 inverse mismatch", error);
    }
};

class LimitsCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/limits"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Interactivity::Runtime runtime;
        Interactivity::Limits limits;
        limits.max_nodes = 123u;
        limits.max_activations_per_update = 456u;
        runtime.setLimits(limits);
        return Testing::require(runtime.limits().max_nodes == 123u && runtime.limits().max_activations_per_update == 456u,
                                "interactivity limits did not round-trip", error);
    }
};

} // namespace

void registerInteractivity(Testing::Runner& runner)
{
    runner.add<RuntimeCase>();
    runner.add<EventCase>();
    runner.add<PointerCase>();
    runner.add<SequenceOrderCase>();
    runner.add<MatrixCase>();
    runner.add<LimitsCase>();
}

} // namespace Tests
