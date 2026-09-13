#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Interactivity/Interactivity.hpp"
#include "Models/Models.hpp"
#include "Renderer/ModelScene.hpp"

#include <vector>

namespace Tests {
namespace {

struct RuntimeFixture {
    Ecs::World world;
    Renderer::ModelScene::Instance instance;
    Interactivity::Runtime runtime;
    std::string error;

    bool load(const char *path)
    {
        Models::clearCache();
        const Models::ModelHandle model = Models::load(path, &error);
        if (model == Models::INVALID_MODEL) return false;
        if (!Renderer::ModelScene::instantiate(world, model, &instance, {}, &error)) return false;
        return runtime.load(world, instance, &error);
    }
};

bool requireInt(Interactivity::Runtime& runtime, const char *name, double expected, const char *message, std::string& error)
{
    std::vector<double> value;
    return Testing::require(runtime.variable(name, &value) && value.size() == 1u && value[0] == expected, message, error);
}

class DoNStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/flow-do-n-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/do-n.gltf")) { error = fixture.error; return false; }
        return requireInt(fixture.runtime, "countAfterLimit", 2.0, "KHR flow/doN must stop currentCount at n", error) &&
            requireInt(fixture.runtime, "countAfterReset", 0.0, "KHR flow/doN reset must restore currentCount to zero", error) &&
            requireInt(fixture.runtime, "countAfterOne", 1.0, "KHR flow/doN must increment once per in activation", error);
    }
};

class ForStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/flow-for-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/for-state.gltf")) { error = fixture.error; return false; }
        return requireInt(fixture.runtime, "before", 7.0, "KHR flow/for initialIndex configuration mismatch", error) &&
            requireInt(fixture.runtime, "after", 5.0, "KHR flow/for must retain end index after completion", error);
    }
};

class WaitAllStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/flow-wait-all-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/wait-all.gltf")) { error = fixture.error; return false; }
        return requireInt(fixture.runtime, "initial", 3.0, "KHR flow/waitAll initial remainingInputs mismatch", error) &&
            requireInt(fixture.runtime, "afterFirst", 2.0, "KHR flow/waitAll first input mismatch", error) &&
            requireInt(fixture.runtime, "afterDuplicate", 2.0, "KHR flow/waitAll duplicate input changed remainingInputs", error) &&
            requireInt(fixture.runtime, "afterSecondUnique", 1.0, "KHR flow/waitAll second unique input mismatch", error) &&
            requireInt(fixture.runtime, "afterComplete", 0.0, "KHR flow/waitAll completion mismatch", error) &&
            requireInt(fixture.runtime, "afterReset", 3.0, "KHR flow/waitAll reset mismatch", error) &&
            requireInt(fixture.runtime, "outSeen", 1.0, "KHR flow/waitAll out flow mismatch", error) &&
            requireInt(fixture.runtime, "completedSeen", 1.0, "KHR flow/waitAll completed flow mismatch", error) &&
            requireInt(fixture.runtime, "defaultRemaining", 0.0, "KHR flow/waitAll default inputFlows mismatch", error);
    }
};

class MultiGateStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/flow-multi-gate-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/multi-gate.gltf")) { error = fixture.error; return false; }
        return requireInt(fixture.runtime, "lastAfterThree", 2.0, "KHR flow/multiGate lastIndex mismatch", error) &&
            requireInt(fixture.runtime, "routeAfterExhausted", 8.0, "KHR flow/multiGate did not stop after exhausting outputs", error) &&
            requireInt(fixture.runtime, "lastAfterReset", -1.0, "KHR flow/multiGate reset did not restore lastIndex", error) &&
            requireInt(fixture.runtime, "routeAfterReset", 1.0, "KHR flow/multiGate lexicographic/reset routing mismatch", error);
    }
};

} // namespace

void registerInteractivityFlowConformance(Testing::Runner& runner)
{
    runner.add<DoNStateCase>();
    runner.add<ForStateCase>();
    runner.add<WaitAllStateCase>();
    runner.add<MultiGateStateCase>();
}

} // namespace Tests
