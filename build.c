#include <cbuild.h>

void build(C_Build *b)
{
    C_Dependency *horse = c_git(
        b,
        "Horse",
        "https://github.com/xt9y/Horse.git",
        "main"
    );
    c_dep_cbuild(horse, "Horse", C_TARGET_SHARED_LIBRARY);
    c_dep_include(horse, "Sources");

    C_Dependency *imgui = c_git(
        b,
        "imgui",
        "https://github.com/ocornut/imgui.git",
        "v1.92.9b"
    );
    c_dep_header_only(imgui);

    C_Target *game = c_executable(b, "game");
    c_sources(game, "Sources/*.cpp");
    c_sources(game, "Sources/*/*.cpp");
    c_flag(game, "-std=c++20");
    c_link_system(game, "stdc++");
    c_link_system(game, "m");
    c_link_system(game, "dl");
    c_link_system(game, "pthread");
    c_use(game, horse);
    c_use(game, imgui);
    c_default_target(b, game);
}
