#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "UI/UI.hpp"

#include <imgui.h>

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
        if (!UI::beginFrame()) return true;
        ImGui::SetNextWindowPos(ImVec2(32.0f, 32.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(220.0f, 96.0f), ImGuiCond_Always);
        ImGui::Begin("Horse UI regression", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::TextUnformatted("SDL3 + SDL_GPU");
        ImGui::End();
        return true;
    }

    bool verify(Testing::Context&, std::string& error) override
    {
        const ImDrawData *draw = ImGui::GetDrawData();
        return Testing::require(UI::initialized(), "UI lost initialized state", error) &&
            Testing::require(draw && draw->TotalVtxCount > 0, "UI frame produced no draw geometry", error);
    }
};

} // namespace

void registerUi(Testing::Runner& runner)
{
    runner.add<UiFrameCase>();
}

} // namespace Tests
