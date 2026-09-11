#ifndef GAME_DASHCAM_POST_PROCESS_HPP
#define GAME_DASHCAM_POST_PROCESS_HPP

#include "Sources/Renderer/PostProcess.hpp"

namespace Dashcam {

struct Settings {
    bool enabled = true;
    float fov_degrees = 105.0f;
    float capture_fps = 24.0f;
    float virtual_height = 480.0f;

    float barrel_distortion = 0.16f;
    float chromatic_aberration = 0.0035f;
    float vignette = 0.42f;
    float rolling_shutter = 0.018f;
    float dirt = 0.14f;
    float windshield_reflection = 0.08f;

    float exposure = 1.0f;
    float shadow_crush = 0.10f;
    float highlight_clip = 1.30f;
    float desaturation = 0.16f;
    float green_tint = 0.025f;
    float yellow_tint = 0.020f;
    float noise = 0.028f;
    float color_bleed = 0.18f;

    float macroblocking = 0.16f;
    float interlacing = 0.08f;
    float glitch = 0.28f;

    float vibration = 0.16f;
    float inertia = 0.22f;
};

struct Runtime {
    float delta_seconds = 0.0f;
    float time_seconds = 0.0f;
    float speed = 0.0f;
    float turn_rate = 0.0f;
    float acceleration = 0.0f;
    float g_force = 0.0f;
};

void reset(Settings& settings);

class LensPass final : public Renderer::PostProcess::Pass {
public:
    LensPass(Settings& settings, Runtime& runtime);
    ~LensPass() override;
    bool process(Renderer::PostProcess::Frame& frame) override;
    void shutdown() override;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

class SensorPass final : public Renderer::PostProcess::Pass {
public:
    SensorPass(Settings& settings, Runtime& runtime);
    ~SensorPass() override;
    bool process(Renderer::PostProcess::Frame& frame) override;
    void shutdown() override;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

class CompressionPass final : public Renderer::PostProcess::Pass {
public:
    CompressionPass(Settings& settings, Runtime& runtime);
    ~CompressionPass() override;
    bool process(Renderer::PostProcess::Frame& frame) override;
    void shutdown() override;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

class FrameHoldPass final : public Renderer::PostProcess::Pass {
public:
    FrameHoldPass(Settings& settings, Runtime& runtime);
    ~FrameHoldPass() override;
    bool process(Renderer::PostProcess::Frame& frame) override;
    void shutdown() override;

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

} // namespace Dashcam

#endif
