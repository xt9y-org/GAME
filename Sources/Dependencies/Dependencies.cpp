#include "Dependencies.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

#ifndef GAME_ROOT
#define GAME_ROOT "."
#endif

namespace Dependencies {
namespace {

std::filesystem::path cacheRoot()
{
    if (const char *override_path = std::getenv("C_CACHE_DIR"); override_path && *override_path)
        return override_path;

#ifdef __APPLE__
    if (const char *home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / "Library/Caches/c";
#else
    if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg)
        return std::filesystem::path(xdg) / "c";
    if (const char *home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".cache/c";
#endif

    return {};
}

std::string value(const std::string& line)
{
    const std::size_t first = line.find('"');
    const std::size_t last = line.rfind('"');
    if (first == std::string::npos || last == std::string::npos || last <= first) return {};
    return line.substr(first + 1, last - first - 1);
}

std::string resolvedCommit(const char *name)
{
    std::ifstream lock(std::filesystem::path(GAME_ROOT) / "c.lock");
    if (!lock) return {};

    std::string dependency_name;
    std::string resolved;
    std::string line;

    auto matches = [&]() {
        return dependency_name == name && !resolved.empty();
    };

    while (std::getline(lock, line)) {
        if (line == "[[dependency]]") {
            if (matches()) return resolved;
            dependency_name.clear();
            resolved.clear();
            continue;
        }
        if (line.rfind("name = ", 0) == 0) dependency_name = value(line);
        else if (line.rfind("resolved = ", 0) == 0) resolved = value(line);
    }

    return matches() ? resolved : std::string{};
}

bool markerMatches(const std::filesystem::path& source, const std::string& resolved)
{
    std::ifstream marker(source.string() + ".c-ready");
    std::string marker_value;
    return marker && std::getline(marker, marker_value) && marker_value == resolved;
}

} // namespace

std::filesystem::path path(const char *name)
{
    if (!name || !*name) return {};

    const std::string resolved = resolvedCommit(name);
    const std::filesystem::path cache = cacheRoot();
    if (resolved.empty() || cache.empty()) return {};

    const std::filesystem::path sources = cache / "src";
    const std::string prefix = std::string(name) + "-";
    std::error_code error;

    for (std::filesystem::directory_iterator it(sources, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_directory(error)) continue;
        const std::string filename = it->path().filename().string();
        if (filename.rfind(prefix, 0) != 0) continue;
        if (markerMatches(it->path(), resolved)) return it->path();
    }

    return {};
}

} // namespace Dependencies
