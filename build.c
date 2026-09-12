#include <cbuild.h>

static void platform(C_Target *target)
{
#ifdef __APPLE__
    c_define(target, "GL_SILENCE_DEPRECATION");
    c_include(target, "/opt/homebrew/include");
    c_include(target, "/usr/local/include/lwmgl-1.0.0");
    c_link_flag(target, "-L/opt/homebrew/lib");
    c_framework(target, "OpenGL");
    c_framework(target, "Cocoa");
    c_framework(target, "IOKit");
    c_framework(target, "CoreVideo");
    c_link_system(target, "c++");
#else
    c_link_system(target, "GL");
    c_link_system(target, "GLU");
    c_link_system(target, "m");
    c_link_system(target, "dl");
    c_link_system(target, "pthread");
    c_link_system(target, "stdc++");
#endif
    c_link_system(target, "glfw");
}

static void common(
    C_Target *target,
    C_Dependency *horse,
    C_Dependency *imgui)
{
    c_include(target, ".");
    c_include(target, "/usr/local/include/lwcgl-2.9.3");
    c_flag(target, "-std=c++20");
    c_warnings_strict(target);
    c_use(target, horse);
    c_use(target, imgui);
    platform(target);
    c_link_flag(target, "-L/usr/local/lib");
    c_link_flag(target, "-llwcgl");
#ifdef __APPLE__
    c_link_flag(target, "-llwmgl");
#endif
    c_link_flag(target, "-Wl,-rpath,/usr/local/lib");
}

void build(C_Build *b)
{
    C_Dependency *horse = c_git(
        b,
        "Horse",
        "https://github.com/xt9y/Horse.git",
        "systems"
    );
    c_dep_cbuild(horse, "Horse", C_TARGET_SHARED_LIBRARY);
    c_dep_include(horse, ".");
    c_dep_include(horse, "Sources");

    C_Dependency *imgui = c_git(
        b,
        "imgui",
        "https://github.com/ocornut/imgui.git",
        "v1.92.9b"
    );
    c_dep_header_only(imgui);
    c_dep_include(imgui, ".");

    C_Target *game = c_executable(b, "game");
    c_sources(game, "main.cpp");
    common(game, horse, imgui);

    C_Target *testing = c_executable(b, "testing");
    c_sources(testing, "Testing.cpp");
    c_sources(testing, "Tests/RegisterAll.cpp");
    c_sources(testing, "Tests/Harness/Testing.cpp");
    c_sources(testing, "Tests/Fixtures/RendererFixture.cpp");
    c_sources(testing, "Tests/Core/Ecs/EcsCases.cpp");
    c_sources(testing, "Tests/Input/Semantics/InputCases.cpp");
    c_sources(testing, "Tests/Camera/Projection/CameraCases.cpp");
    c_sources(testing, "Tests/Physics/Queries/PhysicsCases.cpp");
    c_sources(testing, "Tests/Audio/Mixer/AudioCases.cpp");
    c_sources(testing, "Tests/Animation/Runtime/AnimationCases.cpp");
    c_sources(testing, "Tests/Font/Text/FontCases.cpp");
    c_sources(testing, "Tests/Interactivity/Runtime/InteractivityCases.cpp");
    c_sources(testing, "Tests/Models/Core/ModelCases.cpp");
    c_sources(testing, "Tests/Models/GaussianSplat/GaussianSplatCases.cpp");
    c_sources(testing, "Tests/Rendering/Hierarchy/HierarchyCases.cpp");
    c_sources(testing, "Tests/Rendering/Visibility/VisibilityCases.cpp");
    c_sources(testing, "Tests/Rendering/Settings/SettingsCases.cpp");
    c_sources(testing, "Tests/Rendering/Scenes/SceneCases.cpp");
    c_sources(testing, "Tests/UI/Integration/UiCases.cpp");
    common(testing, horse, imgui);

    c_default_target(b, game);
}
