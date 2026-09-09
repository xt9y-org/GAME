#include <cbuild.h>

static void configurePlatform(C_Target *target)
{
#ifdef __APPLE__
    c_define(target, "GL_SILENCE_DEPRECATION");
    c_include(target, "/opt/homebrew/include");
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

static void configureViewer(C_Target *target, C_Dependency *horse, const char *source)
{
    c_sources(target, source);
    c_include(target, "/usr/local/include/lwcgl-2.9.3");
    c_flag(target, "-std=c++20");
    c_warnings_strict(target);

    c_use(target, horse);

    configurePlatform(target);
    c_link_flag(target, "-L/usr/local/lib");
    c_link_flag(target, "-llwcgl");
    c_link_flag(target, "-Wl,-rpath,/usr/local/lib");
}

void build(C_Build *b)
{
    C_Dependency *horse = c_git(
        b,
        "Horse",
        "https://github.com/xt9y/Horse.git",
        "main"
    );
    c_dep_cbuild(horse, "Horse", C_TARGET_SHARED_LIBRARY);
    c_dep_include(horse, ".");
    c_dep_include(horse, "Sources");

    C_Target *sponza = c_test(b, "sponza");
    configureViewer(sponza, horse, "Examples/sponza.cpp");

    C_Target *earth = c_test(b, "earth");
    configureViewer(earth, horse, "Examples/earth.cpp");

    C_Target *earth_texture_contract = c_test(b, "earth-texture-contract");
    configureViewer(earth_texture_contract, horse, "tests/earth_texture_contract.cpp");

    C_Target *earth_sphere_contract = c_test(b, "earth-sphere-contract");
    configureViewer(earth_sphere_contract, horse, "tests/earth_sphere_contract.cpp");

    C_Target *frame_stats_contract = c_test(b, "frame-stats-contract");
    configureViewer(frame_stats_contract, horse, "tests/frame_stats_contract.cpp");

    C_Target *earth_sun_orbit_contract = c_test(b, "earth-sun-orbit-contract");
    c_sources(earth_sun_orbit_contract, "tests/earth_sun_orbit_contract.cpp");
    c_flag(earth_sun_orbit_contract, "-std=c++20");
    c_warnings_strict(earth_sun_orbit_contract);
#ifdef __APPLE__
    c_link_system(earth_sun_orbit_contract, "c++");
#else
    c_link_system(earth_sun_orbit_contract, "stdc++");
#endif

    C_Target *global_illumination_contract = c_test(b, "global-illumination-contract");
    c_sources(global_illumination_contract, "tests/global_illumination_contract.cpp");
    c_flag(global_illumination_contract, "-std=c++20");
    c_warnings_strict(global_illumination_contract);
#ifdef __APPLE__
    c_link_system(global_illumination_contract, "c++");
#else
    c_link_system(global_illumination_contract, "stdc++");
#endif

    C_Target *cursor_toggle_contract = c_test(b, "cursor-toggle-contract");
    c_sources(cursor_toggle_contract, "tests/cursor_toggle_contract.cpp");
    c_flag(cursor_toggle_contract, "-std=c++20");
    c_warnings_strict(cursor_toggle_contract);
#ifdef __APPLE__
    c_link_system(cursor_toggle_contract, "c++");
#else
    c_link_system(cursor_toggle_contract, "stdc++");
#endif

    c_default_target(b, sponza);
}
