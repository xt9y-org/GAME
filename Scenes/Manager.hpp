#ifndef GAME_SCENES_MANAGER_HPP
#define GAME_SCENES_MANAGER_HPP

#include "Scenes/Scene.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Game::Scenes {

class Manager {
public:
    template <typename T, typename... Args>
    T& add(Args&&... args)
    {
        auto scene = std::make_unique<T>(std::forward<Args>(args)...);
        T& reference = *scene;
        names_.push_back(scene->name());
        scenes_.push_back(std::move(scene));
        return reference;
    }

    std::size_t count() const
    {
        return scenes_.size();
    }

    const char *const *names() const
    {
        return names_.empty() ? nullptr : names_.data();
    }

    Scene *at(std::size_t index)
    {
        return index < scenes_.size() ? scenes_[index].get() : nullptr;
    }

    const Scene *at(std::size_t index) const
    {
        return index < scenes_.size() ? scenes_[index].get() : nullptr;
    }

    Scene *current()
    {
        return at(current_);
    }

    const Scene *current() const
    {
        return at(current_);
    }

    std::size_t currentIndex() const
    {
        return current_;
    }

    bool activate(std::size_t index)
    {
        if (index >= scenes_.size()) return false;
        current_ = index;
        return true;
    }

private:
    std::vector<std::unique_ptr<Scene>> scenes_;
    std::vector<const char *> names_;
    std::size_t current_ = 0u;
};

} // namespace Game::Scenes

#endif
