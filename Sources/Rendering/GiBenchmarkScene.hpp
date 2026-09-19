#ifndef GAME_RENDERING_GI_BENCHMARK_SCENE_HPP
#define GAME_RENDERING_GI_BENCHMARK_SCENE_HPP

#include <filesystem>

namespace Rendering::GiBenchmarkScene {

enum class Source {
    Synthetic,
    Default,
    Environment,
    Argument,
};

struct Selection {
    std::filesystem::path path;
    Source source = Source::Synthetic;

    bool generated() const
    {
        return source == Source::Synthetic;
    }
};

inline Selection select(
    const std::filesystem::path& argument,
    const std::filesystem::path& environment,
    const std::filesystem::path& fallback,
    bool fallback_exists)
{
    if (!argument.empty()) return {argument, Source::Argument};
    if (!environment.empty()) return {environment, Source::Environment};
    if (fallback_exists) return {fallback, Source::Default};
    return {};
}

inline const char *sourceName(Source source)
{
    switch (source) {
    case Source::Synthetic: return "synthetic";
    case Source::Default: return "default";
    case Source::Environment: return "environment";
    case Source::Argument: return "argument";
    }
    return "unknown";
}

} // namespace Rendering::GiBenchmarkScene

#endif
