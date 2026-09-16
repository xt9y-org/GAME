#include <cbuild.h>

#include <unistd.h>

static void define_root(C_Target *target)
{
    char root[C_MAX_PATH] = ".";
    (void)getcwd(root, sizeof(root));

    char escaped[C_MAX_PATH * 2];
    size_t out = 0;
    for (size_t i = 0; root[i] && out + 2 < sizeof(escaped); ++i) {
        if (root[i] == '\\' || root[i] == '"') escaped[out++] = '\\';
        escaped[out++] = root[i];
    }
    escaped[out] = '\0';

    char define[C_MAX_PATH * 2 + 32];
    snprintf(define, sizeof(define), "GAME_ROOT=\"%s\"", escaped);
    c_define(target, define);
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
    c_dep_include(horse, "Sources");

    C_Dependency *sponza = c_git(
        b,
        "Sponza",
        "https://github.com/xt9y-org/Sponza.git",
        "master"
    );
    c_dep_header_only(sponza);

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
    c_use(game, sponza);
    c_use(game, imgui);
    define_root(game);
    c_default_target(b, game);
}
