#include "Debugging.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace Debugging {
namespace {

constexpr ImVec4 heading_color{1.0f, 0.82f, 0.0f, 1.0f};
constexpr ImVec4 profiler_background{0.02f, 0.03f, 0.28f, 0.96f};
constexpr ImVec4 profiler_border{0.38f, 0.62f, 1.0f, 1.0f};
constexpr ImVec4 profiler_plot{1.0f, 0.9f, 0.0f, 1.0f};

void disabled(const char *label)
{
    ImGui::MenuItem(label, nullptr, false, false);
}

void emptyMenu(const char *label)
{
    if (!ImGui::BeginMenu(label)) return;
    disabled("No actions");
    ImGui::EndMenu();
}

void section(const char *label)
{
    ImGui::Spacing();
    ImGui::TextColored(heading_color, "%s", label);
    ImGui::Separator();
}

void disabledSubmenu(const char *label)
{
    if (!ImGui::BeginMenu(label)) return;
    disabled("No debug views");
    ImGui::EndMenu();
}

void drawTopBar(State& state)
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Demo")) {
        ImGui::MenuItem("ImGui Demo", nullptr, &state.show_imgui_demo);
        ImGui::EndMenu();
    }

    emptyMenu("File");
    emptyMenu("Editors");
    emptyMenu("Tools");
    emptyMenu("Options");

    if (ImGui::BeginMenu("Debug")) {
        ImGui::TextColored(heading_color, "%s", "Console");
        ImGui::Separator();
        ImGui::MenuItem("Console", nullptr, &state.show_console);

        section("Drawing");
        disabled("Sim Objects");
        disabled("Sim Objects Model Statistics");
        disabled("World Objects");
        disabled("POI");
        disabledSubmenu("Collisions");
        disabled("Flight Object Debug");
        disabledSubmenu("Aircraft");
        disabledSubmenu("Airports");
        disabledSubmenu("Terrain");

        section("Rendering");
        ImGui::MenuItem("Display FPS", nullptr, &state.show_fps);
        disabled("Wireframe");
        ImGui::MenuItem("Display position", nullptr, &state.show_position);
        disabled("Debug model LODs");

        section("WASM");
        disabled("Display WASM Debug Window");

        section("Experimental");
        disabled("Debug Weather");
        disabled("Debug Vegetation");

        ImGui::EndMenu();
    }

    emptyMenu("Camera");
    emptyMenu("Help");

    ImGui::EndMainMenuBar();
}

float graphMaximum(const State& state)
{
    const std::size_t count = state.frame_history_filled
        ? State::FRAME_HISTORY
        : state.frame_index;

    float maximum = 16.67f;
    for (std::size_t i = 0; i < count; ++i)
        maximum = std::max(maximum, state.frame_ms[i]);

    return maximum * 1.15f;
}

void metric(const char *label, float milliseconds)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::Text("%.2f ms", milliseconds);
}

void drawPerformance(State& state)
{
    if (!state.show_fps) return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - 8.0f, ImGui::GetFrameHeight() + 8.0f),
        ImGuiCond_FirstUseEver,
        ImVec2(1.0f, 0.0f)
    );
    ImGui::SetNextWindowSize(ImVec2(350.0f, 360.0f), ImGuiCond_FirstUseEver);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_Border, profiler_border);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, profiler_background);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, profiler_plot);

    if (ImGui::Begin(
            "Performance",
            &state.show_fps,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(1.65f);
        ImGui::Text("%.1f FPS", state.fps);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Text("%.2f ms", state.frame_ms_current);
        ImGui::Separator();

        const int count = static_cast<int>(state.frame_history_filled
            ? State::FRAME_HISTORY
            : state.frame_index);
        const int offset = state.frame_history_filled
            ? static_cast<int>(state.frame_index)
            : 0;

        if (count > 0) {
            char overlay[64]{};
            std::snprintf(overlay, sizeof(overlay), "Frame %.2f ms", state.frame_ms_current);
            ImGui::PlotHistogram(
                "##FrameHistory",
                state.frame_ms.data(),
                count,
                offset,
                overlay,
                0.0f,
                graphMaximum(state),
                ImVec2(0.0f, 92.0f)
            );
        }

        ImGui::Separator();
        ImGui::TextColored(heading_color, "%s", "MainThread");
        metric("Frame", state.frame_ms_current);
        metric("Update", state.update_ms);
        metric("Renderer", state.render_ms);
        metric("Debug UI", state.ui_ms);
    }
    ImGui::End();

    ImGui::PopStyleColor(5);
}

void drawPosition(State& state, Position position)
{
    if (!state.show_position) return;

    ImGui::SetNextWindowSize(ImVec2(250.0f, 130.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Position", &state.show_position, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("X  %.3f", position.x);
        ImGui::Text("Y  %.3f", position.y);
        ImGui::Text("Z  %.3f", position.z);
    }
    ImGui::End();
}

void drawConsole(State& state)
{
    if (!state.show_console) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f, 240.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Console", &state.show_console, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("[GAME] Debug console");
        ImGui::Separator();
        ImGui::TextDisabled("Console logging can be connected here later.");
    }
    ImGui::End();
}

} // namespace

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.06f, 0.97f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.04f, 0.04f, 0.05f, 0.98f);
    colors[ImGuiCol_Header] = ImVec4(0.12f, 0.18f, 0.28f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.30f, 0.48f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.36f, 0.58f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.30f, 0.34f, 1.0f);
}

void sample(State& state, float delta_seconds)
{
    const float milliseconds = delta_seconds > 0.0f
        ? delta_seconds * 1000.0f
        : 0.0f;

    state.frame_ms_current = milliseconds;
    state.fps = delta_seconds > 0.0f ? 1.0f / delta_seconds : 0.0f;

    state.frame_ms[state.frame_index] = milliseconds;
    state.frame_index++;
    if (state.frame_index == State::FRAME_HISTORY) {
        state.frame_index = 0;
        state.frame_history_filled = true;
    }
}

void draw(State& state, Position position)
{
    drawTopBar(state);
    drawPerformance(state);
    drawPosition(state, position);
    drawConsole(state);

    if (state.show_imgui_demo)
        ImGui::ShowDemoWindow(&state.show_imgui_demo);
}

} // namespace Debugging
