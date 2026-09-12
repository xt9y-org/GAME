#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "UI/UI.hpp"

namespace Tests {
namespace {

class UiFrameCase final : public Testing::Case {
public:
    std::string_view name() const override { return "ui/frame"; }
    Testing::Kind kind() const override { return Testing::Kind::Visual; }
    std::size_t frameCount() const override { return 2u; }

    bool setup(Testing::Context&, std::string& error) override
    {
        return Testing::require(UI::init(), "UI initialization failed", error);
    }

    bool update(Testing::Context&, double, std::string&) override
    {
        (void)UI::beginFrame();
        return true;
    }

    bool verify(Testing::Context&, std::string& error) override
    {
        return Testing::require(UI::initialized(), "UI lost initialized state", error);
    }
};

} // namespace

void registerUi(Testing::Runner& runner)
{
    runner.add<UiFrameCase>();
}

} // namespace Tests
