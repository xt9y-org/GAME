#ifndef GAME_TESTS_RENDERERCHECK_HPP
#define GAME_TESTS_RENDERERCHECK_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Renderer {
class Rasterizer;
}

namespace Game::Tests {

class RendererCheck {
public:
    RendererCheck();

    bool active() const { return active_; }
    bool visual() const { return !test_.empty(); }
    bool performance() const { return !performance_case_.empty(); }
    std::string_view test() const { return test_; }
    std::string_view performanceCase() const { return performance_case_; }

    bool captureDue(std::uint64_t frame) const;
    bool lastFrame(std::uint64_t frame) const;
    bool captureOpenGL(int width, int height) const;
    void configure(Renderer::Rasterizer& rasterizer) const;
    void record(const Renderer::Rasterizer& rasterizer) const;
    void metric(std::string_view name, double value) const;

private:
    bool active_ = false;
    std::string_view test_{};
    std::string_view performance_case_{};
    const char *capture_path_ = nullptr;
    const char *metrics_path_ = nullptr;
    std::uint64_t capture_frame_ = 0u;
    std::uint64_t frame_limit_ = 0u;
};

} // namespace Game::Tests

#endif
