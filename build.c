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

    C_Target *motion_test = c_test(b, "viewmodel-motion-test");
    c_sources(motion_test, "Tests/ViewmodelMotion.cpp");
    c_sources(motion_test, "Sources/Viewmodel/Motion.cpp");
    c_include(motion_test, "Sources");
    c_flag(motion_test, "-std=c++20");
    c_link_system(motion_test, "stdc++");
    c_link_system(motion_test, "m");
    c_link_system(motion_test, "dl");
    c_link_system(motion_test, "pthread");
    c_use(motion_test, horse);

    C_Target *fire_test = c_test(b, "loadout-fire-test");
    c_sources(fire_test, "Tests/LoadoutFire.cpp");
    c_sources(fire_test, "Sources/Loadout/Fire.cpp");
    c_include(fire_test, "Sources");
    c_flag(fire_test, "-std=c++20");
    c_link_system(fire_test, "stdc++");
    c_link_system(fire_test, "m");
}
