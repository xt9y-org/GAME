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
    c_include(game, "/usr/local/include/lwcgl-2.9.3");
    c_flag(game, "-std=c++20");
    c_warnings_strict(game);
    c_use(game, horse);
    c_use(game, imgui);
    platform(game);
    c_link_flag(game, "-L/usr/local/lib");
    c_link_flag(game, "-llwcgl");
#ifdef __APPLE__
    c_link_flag(game, "-llwmgl");
#endif
    c_link_flag(game, "-Wl,-rpath,/usr/local/lib");
    c_default_target(b, game);
}
