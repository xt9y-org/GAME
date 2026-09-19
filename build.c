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

    C_Target *renderer_setup_test = c_test(b, "renderer-setup-test");
    c_sources(renderer_setup_test, "Tests/RendererSetup.cpp");
    c_sources(renderer_setup_test, "Sources/Rendering/Setup.cpp");
    c_include(renderer_setup_test, "Sources");
    c_flag(renderer_setup_test, "-std=c++20");
    c_link_system(renderer_setup_test, "stdc++");
    c_link_system(renderer_setup_test, "m");
    c_link_system(renderer_setup_test, "dl");
    c_link_system(renderer_setup_test, "pthread");
    c_use(renderer_setup_test, horse);

    C_Target *gi_metrics_test = c_test(b, "debugging-gi-metrics-test");
    c_sources(gi_metrics_test, "Tests/DebuggingGiMetrics.cpp");
    c_flag(gi_metrics_test, "-std=c++20");
    c_link_system(gi_metrics_test, "stdc++");

    C_Target *debug_layout_test = c_test(b, "debug-layout-test");
    c_sources(debug_layout_test, "Tests/DebugLayout.cpp");
    c_include(debug_layout_test, "Sources");
    c_flag(debug_layout_test, "-std=c++20");
    c_link_system(debug_layout_test, "stdc++");
    c_use(debug_layout_test, horse);

    C_Target *forward_plus_benchmark_test = c_test(b, "forward-plus-benchmark-test");
    c_sources(forward_plus_benchmark_test, "Tests/ForwardPlusBenchmark.cpp");
    c_include(forward_plus_benchmark_test, "Sources");
    c_flag(forward_plus_benchmark_test, "-std=c++20");
    c_link_system(forward_plus_benchmark_test, "stdc++");

    C_Target *volumetrics_benchmark_test = c_test(b, "volumetrics-benchmark-test");
    c_sources(volumetrics_benchmark_test, "Tests/VolumetricsBenchmark.cpp");
    c_include(volumetrics_benchmark_test, "Sources");
    c_flag(volumetrics_benchmark_test, "-std=c++20");
    c_link_system(volumetrics_benchmark_test, "stdc++");

    C_Target *gi_benchmark_test = c_test(b, "gi-benchmark-test");
    c_sources(gi_benchmark_test, "Tests/GlobalIlluminationBenchmark.cpp");
    c_include(gi_benchmark_test, "Sources");
    c_flag(gi_benchmark_test, "-std=c++20");
    c_link_system(gi_benchmark_test, "stdc++");

    C_Target *forward_plus_benchmark = c_executable(b, "forward-plus-benchmark");
    c_sources(forward_plus_benchmark, "Benchmarks/ForwardPlus.cpp");
    c_include(forward_plus_benchmark, "Sources");
    c_flag(forward_plus_benchmark, "-std=c++20");
    c_link_system(forward_plus_benchmark, "stdc++");
    c_link_system(forward_plus_benchmark, "m");
    c_link_system(forward_plus_benchmark, "dl");
    c_link_system(forward_plus_benchmark, "pthread");
    c_use(forward_plus_benchmark, horse);

    C_Target *volumetrics_benchmark = c_executable(b, "volumetrics-benchmark");
    c_sources(volumetrics_benchmark, "Benchmarks/Volumetrics.cpp");
    c_include(volumetrics_benchmark, "Sources");
    c_flag(volumetrics_benchmark, "-std=c++20");
    c_link_system(volumetrics_benchmark, "stdc++");
    c_link_system(volumetrics_benchmark, "m");
    c_link_system(volumetrics_benchmark, "dl");
    c_link_system(volumetrics_benchmark, "pthread");
    c_use(volumetrics_benchmark, horse);

    C_Target *gi_benchmark = c_executable(b, "gi-benchmark");
    c_sources(gi_benchmark, "Benchmarks/GlobalIllumination.cpp");
    c_include(gi_benchmark, "Sources");
    c_flag(gi_benchmark, "-std=c++20");
    c_link_system(gi_benchmark, "stdc++");
    c_link_system(gi_benchmark, "m");
    c_link_system(gi_benchmark, "dl");
    c_link_system(gi_benchmark, "pthread");
    c_use(gi_benchmark, horse);

    C_Target *shadow_benchmark = c_executable(b, "shadow-benchmark");
    c_sources(shadow_benchmark, "Benchmarks/Shadows.cpp");
    c_include(shadow_benchmark, "Sources");
    c_flag(shadow_benchmark, "-std=c++20");
    c_link_system(shadow_benchmark, "stdc++");
    c_link_system(shadow_benchmark, "m");
    c_link_system(shadow_benchmark, "dl");
    c_link_system(shadow_benchmark, "pthread");
    c_use(shadow_benchmark, horse);
}
