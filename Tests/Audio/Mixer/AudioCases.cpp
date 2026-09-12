#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Audio/Audio.hpp"
#include "Renderer/Components.hpp"

namespace Tests {
namespace {

Audio::ClipHandle clip()
{
    return Audio::registerClip(Audio::Clip{
        .sample_rate = 4u,
        .channels = 1u,
        .samples = {1.0f, 0.5f, -0.5f, -1.0f},
    });
}

class ClipCase final : public Testing::Case {
public:
    std::string_view name() const override { return "audio/clip"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Audio::clearClips();
        const Audio::ClipHandle handle = clip();
        const Audio::Clip *value = Audio::clip(handle);
        return Testing::require(handle != Audio::INVALID_CLIP && value, "clip registration failed", error) &&
            Testing::require(value->frameCount() == 4u, "clip frame count mismatch", error);
    }
};

class PlaybackCase final : public Testing::Case {
public:
    std::string_view name() const override { return "audio/playback"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Audio::clearClips();
        Audio::SourceComponent source{.clip = clip()};
        Audio::play(source);
        if (!Testing::require(source.playing && Testing::near(source.frame, 0.0), "play failed", error)) return false;
        source.frame = 2.0;
        Audio::pause(source);
        if (!Testing::require(!source.playing && Testing::near(source.frame, 2.0), "pause changed frame", error)) return false;
        Audio::play(source, false);
        if (!Testing::require(source.playing && Testing::near(source.frame, 2.0), "resume failed", error)) return false;
        Audio::stop(source);
        return Testing::require(!source.playing && Testing::near(source.frame, 0.0), "stop failed", error);
    }
};

class MixerCase final : public Testing::Case {
public:
    std::string_view name() const override { return "audio/mixer"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Audio::clearClips();
        Ecs::World world;
        const Ecs::Entity source_entity = world.createEntity();
        world.add<Renderer::Transform>(source_entity, Renderer::Transform{});
        Audio::SourceComponent& source = world.add<Audio::SourceComponent>(source_entity, Audio::SourceComponent{
            .clip = clip(), .spatial = false,
        });
        Audio::play(source);
        Audio::Mixer mixer(4u, 1u);
        const std::vector<float> output = mixer.mix(world, 4u);
        return Testing::require(output.size() == 4u, "mixer output size mismatch", error) &&
            Testing::require(Testing::near(output[0], 1.0f) && Testing::near(output[1], 0.5f) &&
                             Testing::near(output[2], -0.5f) && Testing::near(output[3], -1.0f),
                             "mixer samples mismatch", error);
    }
};

class SpatialCase final : public Testing::Case {
public:
    std::string_view name() const override { return "audio/spatial"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Audio::clearClips();
        Ecs::World world;
        const Ecs::Entity listener = world.createEntity();
        world.add<Renderer::Transform>(listener, Renderer::Transform{});
        world.add<Audio::ListenerComponent>(listener, Audio::ListenerComponent{});

        const Ecs::Entity source_entity = world.createEntity();
        world.add<Renderer::Transform>(source_entity, Renderer::Transform{.position = {1.0f, 0.0f, 0.0f}});
        Audio::SourceComponent& source = world.add<Audio::SourceComponent>(source_entity, Audio::SourceComponent{
            .clip = clip(), .min_distance = 0.1f, .max_distance = 10.0f, .spatial = true,
        });
        Audio::play(source);
        Audio::Mixer mixer(4u, 2u);
        const std::vector<float> output = mixer.mix(world, 1u);
        return Testing::require(output.size() == 2u, "spatial mixer output size mismatch", error) &&
            Testing::require(output[0] != output[1], "spatial panning did not separate channels", error);
    }
};

} // namespace

void registerAudio(Testing::Runner& runner)
{
    runner.add<ClipCase>();
    runner.add<PlaybackCase>();
    runner.add<MixerCase>();
    runner.add<SpatialCase>();
}

} // namespace Tests
