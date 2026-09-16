#ifndef GAME_SHOWCASE_DISCOVERY_HPP
#define GAME_SHOWCASE_DISCOVERY_HPP

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace Showcase::Discovery {

inline bool ignored(const std::filesystem::path& path)
{
    const std::string stem = path.stem().string();
    return stem.find("_physics") != std::string::npos ||
           stem.find("_mag") != std::string::npos;
}

inline std::filesystem::path primaryModel(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> models;
    std::error_code error;

    for (std::filesystem::directory_iterator it(directory, error), end;
         !error && it != end;
         it.increment(error)) {
        if (!it->is_regular_file(error)) continue;
        const std::filesystem::path path = it->path();
        if (path.extension() != ".gltf" || ignored(path)) continue;
        models.push_back(path);
    }

    if (models.empty()) return {};

    std::sort(models.begin(), models.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });

    const std::string directory_name = directory.filename().string();
    for (const auto& model : models)
        if (model.stem().string() == directory_name) return model;

    for (const auto& model : models)
        if (model.filename().string().rfind("weapon_", 0) == 0) return model;

    return models.front();
}

inline bool excluded(
    std::string_view name,
    std::initializer_list<std::string_view> exclusions
)
{
    for (const std::string_view exclusion : exclusions)
        if (name == exclusion) return true;
    return false;
}

inline std::vector<std::filesystem::path> childModels(
    const std::filesystem::path& root,
    std::initializer_list<std::string_view> exclusions = {}
)
{
    std::vector<std::filesystem::path> directories;
    std::error_code error;

    for (std::filesystem::directory_iterator it(root, error), end;
         !error && it != end;
         it.increment(error)) {
        if (!it->is_directory(error)) continue;
        if (excluded(it->path().filename().string(), exclusions)) continue;
        directories.push_back(it->path());
    }

    std::sort(directories.begin(), directories.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });

    std::vector<std::filesystem::path> models;
    models.reserve(directories.size());
    for (const auto& directory : directories) {
        const std::filesystem::path model = primaryModel(directory);
        if (!model.empty()) models.push_back(model);
    }
    return models;
}

inline float centeredX(std::size_t index, std::size_t count, float spacing)
{
    if (count == 0) return 0.0f;
    return (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * spacing;
}

} // namespace Showcase::Discovery

#endif
