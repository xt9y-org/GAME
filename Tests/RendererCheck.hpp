#ifndef GAME_TESTS_RENDERERCHECK_HPP
#define GAME_TESTS_RENDERERCHECK_HPP

#include <cstdint>
#include <string_view>

namespace Game::Tests {

class RendererCheck {
public:
    RendererCheck();

    bool active() const { return active_; }
    std::string_view rendererName() const { return renderer_name_; }
    bool lastFrame(std::uint64_t frame) const;
    void metric(std::string_view name, double value) const;

private:
    bool active_ = false;
    const char *metrics_path_ = nullptr;
    std::uint64_t frame_limit_ = 0u;
    std::string_view renderer_name_ = "Rasterizer";
};

} // namespace Game::Tests

#endif
