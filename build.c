#include <cbuild.h>

static void configurePlatform(C_Target *target)
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

static void configureDrivingAssets(C_Target *target)
{
    c_generate(
        target,
        "build/generated/driving_assets.cpp",
        "build.c",
        "mkdir -p build/generated Assets/Driving/Cars Assets/Driving/Street "
        "Assets/Driving/Foliage Assets/Driving/City; "
        "fetch() { "
            "url=\"$1\"; dst=\"$2\"; "
            "[ -s \"$dst\" ] && return 0; "
            "if ! command -v curl >/dev/null 2>&1; then "
                "echo '[assets] curl unavailable; using procedural fallback' >&2; return 0; "
            "fi; "
            "echo \"[assets] $dst\"; "
            "if curl -fL --retry 2 --connect-timeout 10 -o \"$dst.part\" \"$url\"; then "
                "mv \"$dst.part\" \"$dst\"; "
            "else "
                "rm -f \"$dst.part\"; "
                "echo \"[assets] failed: $dst; using fallback\" >&2; "
            "fi; "
        "}; "
        "fetch 'https://github.com/xt9y-org/Cars/releases/download/v1.0.0/1967_chevy_camaro_ss_hidden_jewel.glb' "
            "'Assets/Driving/Cars/1967_chevy_camaro_ss_hidden_jewel.glb'; "
        "fetch 'https://github.com/xt9y-org/Cars/releases/download/v1.0.0/2010_mercedes-benz_sls_amg.glb' "
            "'Assets/Driving/Cars/2010_mercedes-benz_sls_amg.glb'; "
        "fetch 'https://github.com/xt9y-org/Cars/releases/download/v1.0.0/2015_mercedes-benz_s65_amg_coupe.glb' "
            "'Assets/Driving/Cars/2015_mercedes-benz_s65_amg_coupe.glb'; "
        "fetch 'https://github.com/xt9y-org/Street/releases/download/v1.0.0/low_poly_street_gameready_6.glb' "
            "'Assets/Driving/Street/low_poly_street_gameready_6.glb'; "
        "fetch 'https://github.com/xt9y-org/Street/releases/download/v1.0.0/road_signs_asset_pack__australian_american.glb' "
            "'Assets/Driving/Street/road_signs_asset_pack__australian_american.glb'; "
        "fetch 'https://github.com/xt9y-org/Foliage/releases/download/v1.0.0/low_poly_stylized_plants_pack_free.glb' "
            "'Assets/Driving/Foliage/low_poly_stylized_plants_pack_free.glb'; "
        "fetch 'https://github.com/xt9y-org/City/releases/download/v1.0.0/street_city_7_for_games_free.glb' "
            "'Assets/Driving/City/street_city_7_for_games_free.glb'; "
        "fetch 'https://github.com/xt9y-org/City/releases/download/v1.0.0/street_city_buildings_8.glb' "
            "'Assets/Driving/City/street_city_buildings_8.glb'; "
        "printf '%s\\n' 'int game_driving_assets_stamp = 0;' > build/generated/driving_assets.cpp"
    );
}

static void configureGame(
    C_Target *target,
    C_Dependency *horse,
    C_Dependency *imgui)
{
    c_sources(target, "main.cpp");
    c_sources(target, "Driving/*.cpp");
    c_sources(target, "Scenes/*.cpp");
    c_sources(target, "Tests/*.cpp");
    c_include(target, ".");
    c_include(target, "/usr/local/include/lwcgl-2.9.3");
    c_flag(target, "-std=c++20");
    c_warnings_strict(target);

    c_use(target, horse);
    c_use(target, imgui);
    configureDrivingAssets(target);

    configurePlatform(target);
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
    configureGame(game, horse, imgui);
    c_default_target(b, game);
}
