#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Interactivity/Interactivity.hpp"
#include "Models/Models.hpp"
#include "Renderer/ModelScene.hpp"

#include <vector>

namespace Tests {
namespace {

class DoNStateCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/flow-do-n-state"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        Models::clearCache();
        std::string runtime_error;
        const Models::ModelHandle model = Models::load("Assets/Interactivity/do-n.gltf", &runtime_error);
        if (model == Models::INVALID_MODEL) {
            error = runtime_error;
            return false;
        }

        Ecs::World world;
        Renderer::ModelScene::Instance instance;
        if (!Renderer::ModelScene::instantiate(world, model, &instance, {}, &runtime_error)) {
            error = runtime_error;
            return false;
        }

        Interactivity::Runtime runtime;
        if (!runtime.load(world, instance, &runtime_error)) {
            error = runtime_error;
            return false;
        }

        std::vector<double> after_limit;
        std::vector<double> after_reset;
        std::vector<double> after_one;
        return Testing::require(
                   runtime.variable("countAfterLimit", &after_limit) && after_limit.size() == 1u && after_limit[0] == 2.0,
                   "KHR flow/doN must stop currentCount at n", error) &&
            Testing::require(
                runtime.variable("countAfterReset", &after_reset) && after_reset.size() == 1u && after_reset[0] == 0.0,
                "KHR flow/doN reset must restore currentCount to zero", error) &&
            Testing::require(
                runtime.variable("countAfterOne", &after_one) && after_one.size() == 1u && after_one[0] == 1.0,
                "KHR flow/doN must increment once per in activation", error);
    }
};

} // namespace

void registerInteractivityFlowConformance(Testing::Runner& runner)
{
    runner.add<DoNStateCase>();
}

} // namespace Tests
