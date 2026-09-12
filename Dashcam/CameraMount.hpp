#ifndef GAME_DASHCAM_CAMERA_MOUNT_HPP
#define GAME_DASHCAM_CAMERA_MOUNT_HPP

#include "Dashcam/PostProcess.hpp"
#include "Sources/Renderer/Components.hpp"

namespace Dashcam {

class CameraMount {
public:
    void update(
        const Renderer::Transform& transform,
        float delta_seconds,
        Runtime& runtime
    );
    void reset();

private:
    Renderer::Vec3 previous_position_{};
    float previous_yaw_ = 0.0f;
    float previous_speed_ = 0.0f;
    bool history_ = false;
};

} // namespace Dashcam

#endif
