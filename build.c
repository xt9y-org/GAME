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

static void configureViewer(C_Target *target, C_Dependency *rasterizer, const char *source)
{
    c_sources(target, source);
    c_include(target, "/usr/local/include/lwcgl-2.9.3");
    c_flag(target, "-std=c++20");
    c_warnings_strict(target);

    c_use(target, rasterizer);

    configurePlatform(target);
    c_link_flag(target, "-L/usr/local/lib");
    c_link_flag(target, "-llwcgl");
    c_link_flag(target, "-Wl,-rpath,/usr/local/lib");
}

void build(C_Build *b)
{
    C_Dependency *rasterizer = c_git(
        b,
        "ecs-model-rasterizer",
        "https://github.com/xt9y/ECS-MODEL-RASTERIZER.git",
        "main"
    );
    c_dep_cbuild(rasterizer, "ecs-model-rasterizer", C_TARGET_SHARED_LIBRARY);
    c_dep_include(rasterizer, ".");
    c_dep_include(rasterizer, "Sources");

    C_Target *sponza = c_test(b, "sponza");
    configureViewer(sponza, rasterizer, "Examples/sponza.cpp");

    C_Target *earth = c_test(b, "earth");
    configureViewer(earth, rasterizer, "Examples/earth.cpp");

    C_Target *earth_sun_orbit_contract = c_test(b, "earth-sun-orbit-contract");
    c_sources(earth_sun_orbit_contract, "tests/earth_sun_orbit_contract.cpp");
    c_flag(earth_sun_orbit_contract, "-std=c++20");
    c_warnings_strict(earth_sun_orbit_contract);
#ifdef __APPLE__
    c_link_system(earth_sun_orbit_contract, "c++");
#else
    c_link_system(earth_sun_orbit_contract, "stdc++");
#endif

    c_default_target(b, sponza);
}
