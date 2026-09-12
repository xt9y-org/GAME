#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Interactivity/Interactivity.hpp"
#include "Models/Models.hpp"
#include "Renderer/ModelScene.hpp"

#include <array>
#include <string_view>
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

bool requireQuat(
    Interactivity::Runtime& runtime,
    std::string_view variable,
    const std::array<double, 4>& expected,
    std::string& error)
{
    std::vector<double> value;
    if (!runtime.variable(variable, &value) || value.size() != 4u)
        return Testing::require(false, std::string(variable) + " quaternion result missing", error);

    for (std::size_t index = 0u; index < expected.size(); ++index) {
        if (!Testing::near(value[index], expected[index], 1.0e-5))
            return Testing::require(false, std::string(variable) + " quaternion order mismatch", error);
    }
    return true;
}

class QuaternionAngleOrdersCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/quaternion-angle-orders"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/quat-orders.gltf")) {
            error = fixture.error;
            return false;
        }

        return requireQuat(fixture.runtime, "xyz", {0.391903847, 0.200562149, 0.531975700, 0.723317400}, error) &&
            requireQuat(fixture.runtime, "xzy", {0.0222600065, 0.200562134, 0.531975700, 0.822363138}, error) &&
            requireQuat(fixture.runtime, "yxz", {0.391903847, 0.200562149, 0.360423400, 0.822363138}, error) &&
            requireQuat(fixture.runtime, "yzx", {0.391903847, 0.439679742, 0.360423400, 0.723317400}, error) &&
            requireQuat(fixture.runtime, "zxy", {0.0222600065, 0.439679742, 0.531975700, 0.723317300}, error) &&
            requireQuat(fixture.runtime, "zyx", {0.0222600121, 0.439679742, 0.360423400, 0.822363138}, error);
    }
};

class EqualitySpecialValuesCase final : public Testing::Case {
public:
    std::string_view name() const override { return "interactivity/equality-special-values"; }

    bool verify(Testing::Context&, std::string& error) override
    {
        RuntimeFixture fixture;
        if (!fixture.load("Assets/Interactivity/eq-special.gltf")) {
            error = fixture.error;
            return false;
        }

        std::vector<double> nan_equals_nan;
        std::vector<double> inf_equals_inf;
        return Testing::require(
                   fixture.runtime.variable("nanEqualsNan", &nan_equals_nan) && nan_equals_nan.size() == 1u && nan_equals_nan[0] == 0.0,
                   "KHR math/eq must report NaN != NaN", error) &&
            Testing::require(
                fixture.runtime.variable("infEqualsInf", &inf_equals_inf) && inf_equals_inf.size() == 1u && inf_equals_inf[0] == 1.0,
                "KHR math/eq must report +Inf == +Inf", error);
    }
};

} // namespace

void registerInteractivityConformance(Testing::Runner& runner)
{
    runner.add<QuaternionAngleOrdersCase>();
    runner.add<EqualitySpecialValuesCase>();
}

} // namespace Tests
