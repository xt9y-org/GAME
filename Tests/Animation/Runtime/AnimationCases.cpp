#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Animation/Animation.hpp"

namespace Tests {
namespace {

Animation::SkeletonHandle skeleton()
{
    Animation::Skeleton value;
    value.name = "one-bone";
    value.bones.push_back(Animation::Bone{.name = "root"});
    return Animation::registerSkeleton(std::move(value));
}

Animation::ClipHandle animation(float x)
{
    Animation::AnimationClip clip;
    clip.duration = 1.0f;
    clip.sample_rate = 1.0f;
    Animation::Track track;
    track.samples = {
        Animation::Transform{.translation = {0.0f, 0.0f, 0.0f}},
        Animation::Transform{.translation = {x, 0.0f, 0.0f}},
    };
    clip.tracks.push_back(std::move(track));
    return Animation::registerClip(std::move(clip));
}

class SkeletonCase final : public Testing::Case {
public:
    std::string_view name() const override { return "animation/skeleton"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Animation::clearAssets();
        const Animation::SkeletonHandle handle = skeleton();
        return Testing::require(Animation::skeleton(handle) != nullptr, "skeleton registration failed", error) &&
            Testing::require(Animation::skeleton(handle)->bones.size() == 1u, "skeleton bone count mismatch", error);
    }
};

class AnimatorCase final : public Testing::Case {
public:
    std::string_view name() const override { return "animation/animator"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Animation::clearAssets();
        Ecs::World world;
        const Ecs::Entity entity = world.createEntity();
        Animation::AnimatorComponent animator{.skeleton = skeleton(), .clip = animation(2.0f)};
        world.add<Animation::AnimatorComponent>(entity, animator);
        Animation::System system;
        system.update(world, 0.5f);
        const auto *result = world.get<Animation::AnimatorComponent>(entity);
        return Testing::require(result && result->pose.local.size() == 1u, "animator pose missing", error) &&
            Testing::require(Testing::near(result->pose.local[0].translation.x, 1.0f), "animation sampling mismatch", error) &&
            Testing::require(result->pose.revision > 0u, "pose revision did not advance", error);
    }
};

class BlendCase final : public Testing::Case {
public:
    std::string_view name() const override { return "animation/blending"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        const Animation::Transform value = Animation::blend(
            Animation::Transform{.translation = {0.0f, 0.0f, 0.0f}},
            Animation::Transform{.translation = {10.0f, 2.0f, -2.0f}},
            0.25f
        );
        return Testing::require(Testing::near(value.translation.x, 2.5f) &&
                                Testing::near(value.translation.y, 0.5f) &&
                                Testing::near(value.translation.z, -0.5f),
                                "transform blend mismatch", error);
    }
};

class StateMachineCase final : public Testing::Case {
public:
    std::string_view name() const override { return "animation/state-machine"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Animation::clearAssets();
        const Animation::ClipHandle clip = animation(1.0f);
        Animation::StateMachine machine;
        machine.add(Animation::State{.name = "walk", .clip = clip, .loop = false, .speed = 2.0f});
        Animation::AnimatorComponent animator;
        const bool played = Animation::playState(animator, machine, "walk");
        return Testing::require(played, "state did not play", error) &&
            Testing::require(animator.clip == clip && !animator.loop && Testing::near(animator.speed, 2.0f),
                             "state settings mismatch", error) &&
            Testing::require(!Animation::playState(animator, machine, "missing"), "missing state unexpectedly played", error);
    }
};

class SkinningCase final : public Testing::Case {
public:
    std::string_view name() const override { return "animation/skinning"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Animation::Pose pose;
        pose.skin.push_back(Animation::matrix(Animation::Transform{.translation = {2.0f, 0.0f, 0.0f}}));
        Animation::SkinWeights weights;
        weights.joints[0] = 0u;
        weights.weights[0] = 1.0f;
        Animation::Vec3 position;
        Animation::Vec3 normal;
        const bool skinned = Animation::skinVertex(pose, weights, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, &position, &normal);
        return Testing::require(skinned, "skinning returned false", error) &&
            Testing::require(Testing::near(position.x, 3.0f), "skinned position mismatch", error) &&
            Testing::require(Testing::near(normal.y, 1.0f), "skinned normal mismatch", error);
    }
};

} // namespace

void registerAnimation(Testing::Runner& runner)
{
    runner.add<SkeletonCase>();
    runner.add<AnimatorCase>();
    runner.add<BlendCase>();
    runner.add<StateMachineCase>();
    runner.add<SkinningCase>();
}

} // namespace Tests
