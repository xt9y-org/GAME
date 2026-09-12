#include "Dashcam/PostProcess.hpp"

#include "Dashcam/Shaders.hpp"

#include <lwcgl/glmodern.h>
#include <lwcgl/lwcgl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <lwmgl/lwmgl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#ifndef GL_RGBA16F_ARB
#define GL_RGBA16F_ARB 0x881A
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0_EXT
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_FRAMEBUFFER_COMPLETE_EXT 0x8CD5
#endif

namespace Dashcam {
namespace {

struct alignas(16) Params {
    std::array<float, 4> p0{};
    std::array<float, 4> p1{};
    std::array<float, 4> p2{};
    std::array<float, 4> p3{};
};

GLuint compileShader(GLenum type, const char *source, const char *label)
{
    if (!GL20.glCreateShader || !GL20.glShaderSource || !GL20.glCompileShader) return 0u;
    const GLuint shader = GL20.glCreateShader(type);
    if (shader == 0u) return 0u;
    GL20.glShaderSource(shader, 1, &source, nullptr);
    GL20.glCompileShader(shader);
    GLint status = GL_FALSE;
    GL20.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) return shader;

    GLint length = 0;
    GL20.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GL20.glGetShaderInfoLog(shader, length, nullptr, log.data());
    std::fprintf(stderr, "[Dashcam/%s]: shader compile failed: %s\n", label, log.data());
    GL20.glDeleteShader(shader);
    return 0u;
}

GLuint createProgram(const char *fragment, const char *label)
{
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, Shaders::vertex, label);
    if (vertex == 0u) return 0u;
    const GLuint pixel = compileShader(GL_FRAGMENT_SHADER, fragment, label);
    if (pixel == 0u) {
        GL20.glDeleteShader(vertex);
        return 0u;
    }

    const GLuint program = GL20.glCreateProgram();
    if (program == 0u) {
        GL20.glDeleteShader(pixel);
        GL20.glDeleteShader(vertex);
        return 0u;
    }
    GL20.glAttachShader(program, vertex);
    GL20.glAttachShader(program, pixel);
    GL20.glLinkProgram(program);
    GL20.glDeleteShader(pixel);
    GL20.glDeleteShader(vertex);

    GLint status = GL_FALSE;
    GL20.glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) return program;

    GLint length = 0;
    GL20.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GL20.glGetProgramInfoLog(program, length, nullptr, log.data());
    std::fprintf(stderr, "[Dashcam/%s]: shader link failed: %s\n", label, log.data());
    GL20.glDeleteProgram(program);
    return 0u;
}

using GenFramebuffersProc = void (*)(GLsizei, GLuint *);
using DeleteFramebuffersProc = void (*)(GLsizei, const GLuint *);
using BindFramebufferProc = void (*)(GLenum, GLuint);
using FramebufferTexture2DProc = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using CheckFramebufferStatusProc = GLenum (*)(GLenum);

GLFWglproc resolveProc(const char *core, const char *extension)
{
    GLFWglproc result = glfwGetProcAddress(core);
    if (!result && extension) result = glfwGetProcAddress(extension);
    return result;
}

class Stage {
public:
    Stage(const char *label, const char *glsl, const char *metal)
        : label_(label), glsl_(glsl), metal_(metal)
    {
    }

    ~Stage() { shutdown(); }

    bool process(Renderer::PostProcess::Frame& frame, const Params& params)
    {
        if (!frame.color_texture) return true;
        if (frame.api == Renderer::PostProcess::GraphicsApi::OpenGL)
            return processOpenGL(frame, params);
#ifdef __APPLE__
        if (frame.api == Renderer::PostProcess::GraphicsApi::Metal)
            return processMetal(frame, params);
#endif
        return false;
    }

    void *output(Renderer::PostProcess::GraphicsApi api) const
    {
        if (api == Renderer::PostProcess::GraphicsApi::OpenGL) {
            return reinterpret_cast<void*>(static_cast<std::uintptr_t>(gl_texture_));
        }
#ifdef __APPLE__
        if (api == Renderer::PostProcess::GraphicsApi::Metal) return metal_texture_;
#endif
        return nullptr;
    }

    bool readyFor(Renderer::PostProcess::GraphicsApi api, int width, int height) const
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (api == Renderer::PostProcess::GraphicsApi::OpenGL)
            return gl_texture_ != 0u && gl_width_ == width && gl_height_ == height;
#ifdef __APPLE__
        if (api == Renderer::PostProcess::GraphicsApi::Metal)
            return metal_texture_ != nullptr && metal_width_ == width && metal_height_ == height;
#endif
        return false;
    }

    void shutdown()
    {
        shutdownOpenGL();
#ifdef __APPLE__
        shutdownMetal();
#endif
    }

private:
    bool initOpenGL()
    {
        if (gl_program_ != 0u) return true;
        if (!lwcglModernGLAvailable() && lwcglLoadModernGL() != 0) return false;

        gl_gen_framebuffers_ = reinterpret_cast<GenFramebuffersProc>(
            resolveProc("glGenFramebuffers", "glGenFramebuffersEXT"));
        gl_delete_framebuffers_ = reinterpret_cast<DeleteFramebuffersProc>(
            resolveProc("glDeleteFramebuffers", "glDeleteFramebuffersEXT"));
        gl_bind_framebuffer_ = reinterpret_cast<BindFramebufferProc>(
            resolveProc("glBindFramebuffer", "glBindFramebufferEXT"));
        gl_framebuffer_texture_2d_ = reinterpret_cast<FramebufferTexture2DProc>(
            resolveProc("glFramebufferTexture2D", "glFramebufferTexture2DEXT"));
        gl_check_framebuffer_ = reinterpret_cast<CheckFramebufferStatusProc>(
            resolveProc("glCheckFramebufferStatus", "glCheckFramebufferStatusEXT"));
        if (!gl_gen_framebuffers_ || !gl_delete_framebuffers_ || !gl_bind_framebuffer_ ||
            !gl_framebuffer_texture_2d_ || !gl_check_framebuffer_)
        {
            return false;
        }

        gl_program_ = createProgram(glsl_, label_);
        if (gl_program_ == 0u) return false;
        gl_source_ = GL20.glGetUniformLocation(gl_program_, "uSource");
        gl_params_[0] = GL20.glGetUniformLocation(gl_program_, "uP0");
        gl_params_[1] = GL20.glGetUniformLocation(gl_program_, "uP1");
        gl_params_[2] = GL20.glGetUniformLocation(gl_program_, "uP2");
        gl_params_[3] = GL20.glGetUniformLocation(gl_program_, "uP3");
        return true;
    }

    void destroyOpenGLTarget()
    {
        if (gl_texture_ != 0u) glDeleteTextures(1, &gl_texture_);
        if (gl_framebuffer_ != 0u && gl_delete_framebuffers_)
            gl_delete_framebuffers_(1, &gl_framebuffer_);
        gl_texture_ = 0u;
        gl_framebuffer_ = 0u;
    }

    bool resizeOpenGL(int width, int height)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (gl_texture_ != 0u && gl_width_ == width && gl_height_ == height) return true;
        destroyOpenGLTarget();

        glGenTextures(1, &gl_texture_);
        if (gl_texture_ == 0u) return false;
        glBindTexture(GL_TEXTURE_2D, gl_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA16F_ARB,
            width,
            height,
            0,
            GL_RGBA,
            GL_FLOAT,
            nullptr
        );

        gl_gen_framebuffers_(1, &gl_framebuffer_);
        if (gl_framebuffer_ == 0u) {
            destroyOpenGLTarget();
            return false;
        }
        gl_bind_framebuffer_(GL_FRAMEBUFFER_EXT, gl_framebuffer_);
        gl_framebuffer_texture_2d_(
            GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT,
            GL_TEXTURE_2D,
            gl_texture_,
            0
        );
        const bool complete =
            gl_check_framebuffer_(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT;
        gl_bind_framebuffer_(GL_FRAMEBUFFER_EXT, 0u);
        glBindTexture(GL_TEXTURE_2D, 0u);
        if (!complete) {
            destroyOpenGLTarget();
            return false;
        }
        gl_width_ = width;
        gl_height_ = height;
        return true;
    }

    bool processOpenGL(Renderer::PostProcess::Frame& frame, const Params& params)
    {
        if (!initOpenGL() || !resizeOpenGL(frame.width, frame.height)) return false;
        const GLuint source = static_cast<GLuint>(
            reinterpret_cast<std::uintptr_t>(frame.color_texture));
        if (source == 0u) return false;

        gl_bind_framebuffer_(GL_FRAMEBUFFER_EXT, gl_framebuffer_);
        glViewport(0, 0, gl_width_, gl_height_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_LIGHTING);
        GL20.glUseProgram(gl_program_);
        if (gl_source_ >= 0) GL20.glUniform1i(gl_source_, 0);
        for (int index = 0; index < 4; ++index) {
            if (gl_params_[index] < 0) continue;
            const std::array<float, 4>& value = index == 0 ? params.p0 :
                (index == 1 ? params.p1 : (index == 2 ? params.p2 : params.p3));
            GL20.glUniform4f(
                gl_params_[index],
                value[0], value[1], value[2], value[3]
            );
        }
        GLModern.glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, source);
        glBegin(GL_TRIANGLES);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f(3.0f, -1.0f);
        glVertex2f(-1.0f, 3.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0u);
        GL20.glUseProgram(0u);
        gl_bind_framebuffer_(GL_FRAMEBUFFER_EXT, 0u);

        frame.color_texture = output(Renderer::PostProcess::GraphicsApi::OpenGL);
        return true;
    }

    void shutdownOpenGL()
    {
        destroyOpenGLTarget();
        if (gl_program_ != 0u && GL20.glDeleteProgram) GL20.glDeleteProgram(gl_program_);
        gl_program_ = 0u;
        gl_source_ = -1;
        gl_params_.fill(-1);
    }

#ifdef __APPLE__
    bool initMetal()
    {
        if (metal_pipeline_) return true;
        if (!Metal.isCreated || Metal.isCreated() == 0) return false;
        metal_library_ = Metal.createLibraryFromSource(metal_, std::strlen(metal_));
        if (!metal_library_) return false;
        metal_function_ = Metal.createFunction(metal_library_, "dashcam_stage");
        if (!metal_function_) return false;
        metal_pipeline_ = Metal.createComputePipeline(metal_function_);
        if (!metal_pipeline_) return false;

        const LWMGLBufferDesc buffer_desc = {sizeof(Params), LWMGL_STORAGE_SHARED};
        Params zero{};
        metal_params_ = Metal.createBuffer(&buffer_desc, &zero);
        const LWMGLSamplerDesc sampler_desc = {
            LWMGL_FILTER_LINEAR,
            LWMGL_FILTER_LINEAR,
            LWMGL_ADDRESS_CLAMP,
            LWMGL_ADDRESS_CLAMP
        };
        metal_sampler_ = Metal.createSampler(&sampler_desc);
        return metal_params_ && metal_sampler_;
    }

    bool resizeMetal(int width, int height)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (metal_texture_ && metal_width_ == width && metal_height_ == height) return true;
        if (metal_texture_) Metal.destroyTexture(metal_texture_);
        metal_texture_ = nullptr;
        const LWMGLTextureDesc desc = {
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            LWMGL_RGBA16_FLOAT,
            LWMGL_TEXTURE_SAMPLED | LWMGL_TEXTURE_READ | LWMGL_TEXTURE_WRITE,
            LWMGL_STORAGE_PRIVATE
        };
        metal_texture_ = Metal.createTexture(&desc);
        if (!metal_texture_) return false;
        metal_width_ = width;
        metal_height_ = height;
        return true;
    }

    bool processMetal(Renderer::PostProcess::Frame& frame, const Params& params)
    {
        if (!initMetal() || !resizeMetal(frame.width, frame.height)) return false;
        LWMGLCommand command = static_cast<LWMGLCommand>(frame.command);
        LWMGLTexture source = static_cast<LWMGLTexture>(frame.color_texture);
        if (!command || !source) return false;
        if (Metal.uploadBuffer(metal_params_, 0u, &params, sizeof params) != 0) return false;

        bool ok = Metal.beginCompute(command) == 0;
        if (ok) ok = Metal.setComputePipeline(command, metal_pipeline_) == 0;
        if (ok) ok = Metal.setBuffer(command, metal_params_, 0u, 0u) == 0;
        if (ok) ok = Metal.setTexture(command, source, 0u) == 0;
        if (ok) ok = Metal.setTexture(command, metal_texture_, 1u) == 0;
        if (ok) ok = Metal.setSampler(command, metal_sampler_, 0u) == 0;
        if (ok) ok = Metal.dispatch(
            command,
            static_cast<std::uint32_t>(metal_width_),
            static_cast<std::uint32_t>(metal_height_),
            1u
        ) == 0;
        if (ok) ok = Metal.endEncoding(command) == 0;
        if (!ok) {
            std::fprintf(stderr, "[Dashcam/%s]: Metal pass failed: %s\n", label_, lwmglGetLastError());
            return false;
        }
        frame.color_texture = metal_texture_;
        return true;
    }

    void shutdownMetal()
    {
        if (metal_texture_) Metal.destroyTexture(metal_texture_);
        if (metal_sampler_) Metal.destroySampler(metal_sampler_);
        if (metal_params_) Metal.destroyBuffer(metal_params_);
        if (metal_pipeline_) Metal.destroyComputePipeline(metal_pipeline_);
        if (metal_function_) Metal.destroyFunction(metal_function_);
        if (metal_library_) Metal.destroyLibrary(metal_library_);
        metal_texture_ = nullptr;
        metal_sampler_ = nullptr;
        metal_params_ = nullptr;
        metal_pipeline_ = nullptr;
        metal_function_ = nullptr;
        metal_library_ = nullptr;
    }
#endif

    const char *label_ = nullptr;
    const char *glsl_ = nullptr;
    const char *metal_ = nullptr;
    GLuint gl_program_ = 0u;
    GLuint gl_framebuffer_ = 0u;
    GLuint gl_texture_ = 0u;
    int gl_width_ = 0;
    int gl_height_ = 0;
    GLint gl_source_ = -1;
    std::array<GLint, 4> gl_params_{{-1, -1, -1, -1}};
    GenFramebuffersProc gl_gen_framebuffers_ = nullptr;
    DeleteFramebuffersProc gl_delete_framebuffers_ = nullptr;
    BindFramebufferProc gl_bind_framebuffer_ = nullptr;
    FramebufferTexture2DProc gl_framebuffer_texture_2d_ = nullptr;
    CheckFramebufferStatusProc gl_check_framebuffer_ = nullptr;

#ifdef __APPLE__
    LWMGLLibrary metal_library_ = nullptr;
    LWMGLFunction metal_function_ = nullptr;
    LWMGLComputePipeline metal_pipeline_ = nullptr;
    LWMGLBuffer metal_params_ = nullptr;
    LWMGLSampler metal_sampler_ = nullptr;
    LWMGLTexture metal_texture_ = nullptr;
    int metal_width_ = 0;
    int metal_height_ = 0;
#endif
};

Params lensParams(const Settings& settings, const Runtime& runtime, const Renderer::PostProcess::Frame& frame)
{
    Params p;
    p.p0 = {
        settings.barrel_distortion,
        settings.chromatic_aberration,
        settings.vignette,
        settings.rolling_shutter,
    };
    p.p1 = {
        settings.dirt,
        settings.windshield_reflection,
        runtime.turn_rate,
        runtime.g_force,
    };
    p.p2 = {
        runtime.time_seconds,
        runtime.speed,
        1.0f / static_cast<float>(std::max(frame.width, 1)),
        1.0f / static_cast<float>(std::max(frame.height, 1)),
    };
    p.p3 = {
        settings.vibration,
        settings.inertia,
        runtime.acceleration,
        0.0f,
    };
    return p;
}

Params sensorParams(const Settings& settings, const Runtime& runtime, const Renderer::PostProcess::Frame& frame)
{
    Params p;
    p.p0 = {
        settings.exposure,
        settings.shadow_crush,
        settings.highlight_clip,
        settings.desaturation,
    };
    p.p1 = {
        settings.green_tint,
        settings.yellow_tint,
        settings.noise,
        settings.color_bleed,
    };
    p.p2 = {
        runtime.time_seconds,
        1.0f / static_cast<float>(std::max(frame.width, 1)),
        1.0f / static_cast<float>(std::max(frame.height, 1)),
        runtime.g_force,
    };
    return p;
}

Params compressionParams(
    const Settings& settings,
    const Runtime& runtime,
    const Renderer::PostProcess::Frame& frame)
{
    Params p;
    p.p0 = {
        settings.virtual_height,
        settings.macroblocking,
        settings.interlacing,
        settings.glitch,
    };
    p.p1 = {
        runtime.time_seconds,
        runtime.g_force,
        runtime.speed,
        static_cast<float>(std::max(frame.width, 1)) /
            static_cast<float>(std::max(frame.height, 1)),
    };
    p.p2 = {
        1.0f / static_cast<float>(std::max(frame.width, 1)),
        1.0f / static_cast<float>(std::max(frame.height, 1)),
        runtime.turn_rate,
        runtime.acceleration,
    };
    return p;
}

} // namespace

void reset(Settings& settings)
{
    settings = Settings{};
}

struct LensPass::Impl {
    Impl(Settings& value, Runtime& state)
        : settings(value), runtime(state), stage("Lens", Shaders::lens_gl, Shaders::lens_metal) {}
    Settings& settings;
    Runtime& runtime;
    Stage stage;
};

LensPass::LensPass(Settings& settings, Runtime& runtime) : impl_(new Impl(settings, runtime)) {}
LensPass::~LensPass() { shutdown(); delete impl_; }
bool LensPass::process(Renderer::PostProcess::Frame& frame)
{
    return !impl_ || !impl_->settings.enabled ||
        impl_->stage.process(frame, lensParams(impl_->settings, impl_->runtime, frame));
}
void LensPass::shutdown() { if (impl_) impl_->stage.shutdown(); }

struct SensorPass::Impl {
    Impl(Settings& value, Runtime& state)
        : settings(value), runtime(state), stage("Sensor", Shaders::sensor_gl, Shaders::sensor_metal) {}
    Settings& settings;
    Runtime& runtime;
    Stage stage;
};

SensorPass::SensorPass(Settings& settings, Runtime& runtime) : impl_(new Impl(settings, runtime)) {}
SensorPass::~SensorPass() { shutdown(); delete impl_; }
bool SensorPass::process(Renderer::PostProcess::Frame& frame)
{
    return !impl_ || !impl_->settings.enabled ||
        impl_->stage.process(frame, sensorParams(impl_->settings, impl_->runtime, frame));
}
void SensorPass::shutdown() { if (impl_) impl_->stage.shutdown(); }

struct CompressionPass::Impl {
    Impl(Settings& value, Runtime& state)
        : settings(value), runtime(state), stage("Compression", Shaders::compression_gl, Shaders::compression_metal) {}
    Settings& settings;
    Runtime& runtime;
    Stage stage;
};

CompressionPass::CompressionPass(Settings& settings, Runtime& runtime) : impl_(new Impl(settings, runtime)) {}
CompressionPass::~CompressionPass() { shutdown(); delete impl_; }
bool CompressionPass::process(Renderer::PostProcess::Frame& frame)
{
    return !impl_ || !impl_->settings.enabled ||
        impl_->stage.process(frame, compressionParams(impl_->settings, impl_->runtime, frame));
}
void CompressionPass::shutdown() { if (impl_) impl_->stage.shutdown(); }

struct FrameHoldPass::Impl {
    Impl(Settings& value, Runtime& state)
        : settings(value), runtime(state), stage("FrameHold", Shaders::copy_gl, Shaders::copy_metal) {}
    Settings& settings;
    Runtime& runtime;
    Stage stage;
    float accumulator = 0.0f;
    bool captured = false;
};

FrameHoldPass::FrameHoldPass(Settings& settings, Runtime& runtime) : impl_(new Impl(settings, runtime)) {}
FrameHoldPass::~FrameHoldPass() { shutdown(); delete impl_; }
bool FrameHoldPass::process(Renderer::PostProcess::Frame& frame)
{
    if (!impl_) return true;
    if (!impl_->settings.enabled || impl_->settings.capture_fps <= 0.0f) {
        impl_->accumulator = 0.0f;
        impl_->captured = false;
        return true;
    }

    const float interval = 1.0f / std::max(impl_->settings.capture_fps, 1.0f);
    impl_->accumulator += std::max(impl_->runtime.delta_seconds, 0.0f);
    bool due = !impl_->captured || impl_->accumulator >= interval;
    if (due && impl_->captured && impl_->runtime.g_force > 0.9f) {
        const float phase = std::fmod(impl_->runtime.time_seconds * 11.0f, 1.0f);
        const float threshold = std::clamp(
            1.0f - (impl_->runtime.g_force - 0.9f) * impl_->settings.glitch * 0.35f,
            0.55f,
            1.0f
        );
        if (phase > threshold) due = false;
    }

    if (due) {
        Params params{};
        if (!impl_->stage.process(frame, params)) return false;
        impl_->captured = true;
        impl_->accumulator = std::fmod(impl_->accumulator, interval);
        return true;
    }

    if (!impl_->stage.readyFor(frame.api, frame.width, frame.height)) {
        Params params{};
        if (!impl_->stage.process(frame, params)) return false;
        impl_->captured = true;
        impl_->accumulator = 0.0f;
        return true;
    }
    frame.color_texture = impl_->stage.output(frame.api);
    return frame.color_texture != nullptr;
}
void FrameHoldPass::shutdown()
{
    if (!impl_) return;
    impl_->stage.shutdown();
    impl_->accumulator = 0.0f;
    impl_->captured = false;
}

} // namespace Dashcam
