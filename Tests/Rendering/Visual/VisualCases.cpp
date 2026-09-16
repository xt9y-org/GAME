#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Tests/Fixtures/RendererFixture.hpp"

#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/Environment.hpp"
#include "Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Renderer/ModelScene.hpp"
#include "Renderer/PostProcess.hpp"

#include <string>
#include <utility>

namespace Tests {
namespace {

enum class Feature {
    DirectionalLight,
    PointLight,
    SpotLight,
    DirectionalShadow,
    PointShadow,
    SpotShadow,
    AdvancedMaterials,
    Environment,
    LinearFog,
    ExponentialFog,
    GlobalIllumination,
    PhotonMapping,
    GaussianSplat,
    PostProcess,
    Everything,
};

bool addModel(Ecs::World& world, const char *path, std::string& error)
{
    std::string load_error;
    const Models::ModelHandle model = Models::load(path, &load_error);
    if (model == Models::INVALID_MODEL) {
        error = std::string("visual model load failed for ") + path + ": " + load_error;
        return false;
    }

    Renderer::ModelScene::Instance instance;
    if (!Renderer::ModelScene::instantiate(world, model, &instance, {}, &load_error)) {
        error = std::string("visual model instantiate failed for ") + path + ": " + load_error;
        return false;
    }
    return true;
}

void addCamera(Ecs::World& world)
{
    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::Transform>(entity, Renderer::Transform{});
    world.add<Camera::CameraComponent>(entity, Camera::CameraComponent{
        .fov_degrees = 60.0f,
        .near_plane = 0.05f,
        .active = true,
        .projection = Camera::Projection::Perspective,
        .far_plane = 100.0f,
    });
}

void addEnvironment(Ecs::World& world, Renderer::FogMode fog = Renderer::FogMode::None)
{
    const Ecs::Entity environment = world.createEntity();
    Renderer::EnvironmentComponent component;
    component.sky_color = {0.035f, 0.055f, 0.095f};
    component.intensity = 1.15f;
    component.rotation_degrees = 17.0f;
    component.ambient_color = {0.85f, 0.9f, 1.0f};
    component.ambient_intensity = 0.12f;
    component.fog = fog;
    component.fog_color = {0.22f, 0.31f, 0.42f};
    component.fog_density = fog == Renderer::FogMode::Exponential ? 0.28f : 0.0f;
    component.fog_start = 2.6f;
    component.fog_end = 7.0f;
    world.add<Renderer::EnvironmentComponent>(environment, component);
}

void addLight(Ecs::World& world, Renderer::LightType type)
{
    const Ecs::Entity light = world.createEntity();
    Renderer::Transform transform;
    Renderer::LightComponent component;
    component.type = type;
    component.color = {1.0f, 0.88f, 0.72f};

    if (type == Renderer::LightType::Directional) {
        transform.rotation = {-28.0f, 32.0f, 0.0f};
        component.intensity = 3.5f;
    } else if (type == Renderer::LightType::Point) {
        transform.position = {1.35f, 1.15f, -1.25f};
        component.intensity = 24.0f;
        component.range = 12.0f;
    } else {
        transform.position = {0.0f, 1.25f, -1.0f};
        transform.rotation = {18.0f, 0.0f, 0.0f};
        component.intensity = 32.0f;
        component.range = 12.0f;
        component.inner_cone_degrees = 18.0f;
        component.outer_cone_degrees = 34.0f;
    }

    world.add<Renderer::Transform>(light, transform);
    world.add<Renderer::LightComponent>(light, component);
    world.add<Renderer::ShadowComponent>(light, Renderer::ShadowComponent{});
}

bool addBaseScene(Ecs::World& world, Renderer::LightType light, std::string& error)
{
    addCamera(world);
    addEnvironment(world);
    addLight(world, light);
    return addModel(world, "Assets/Models/visual-base.obj", error);
}

bool addDepthScene(Ecs::World& world, Renderer::FogMode fog, std::string& error)
{
    addCamera(world);
    addEnvironment(world, fog);
    addLight(world, Renderer::LightType::Directional);
    return addModel(world, "Assets/Models/visual-depth.obj", error);
}

bool addAdvancedMaterials(Ecs::World& world, std::string& error)
{
    addCamera(world);
    addEnvironment(world);
    addLight(world, Renderer::LightType::Point);
    return addModel(world, "Assets/Models/visual-advanced.gltf", error);
}

bool addGaussianSplats(Ecs::World& world, std::string& error)
{
    return addModel(world, "Assets/Models/visual-gaussian.gltf", error);
}

void addGlobalIllumination(Ecs::World& world, bool photons)
{
    Renderer::GlobalIllumination::setRaysPerProbe(8u);
    Renderer::GlobalIllumination::setProbeBudgetPerFrame(4u);
    Renderer::GlobalIllumination::setProbeDimensionRange(2u, 4u);
    Renderer::GlobalIllumination::setMaximumBounces(1u);
    Renderer::GlobalIllumination::setMaximumPhotonCount(128u);
    Renderer::GlobalIllumination::setPaused(false);
    Renderer::GlobalIllumination::reset();

    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::GlobalIlluminationComponent>(entity, Renderer::GlobalIlluminationComponent{
        .enabled = true,
        .intensity = 1.15f,
        .bounces = 1u,
        .photon_mapping = photons,
        .photon_count = photons ? 128u : 0u,
        .photon_radius = photons ? 0.55f : 0.0f,
    });
}

class ProbePass final : public Renderer::PostProcess::Pass {
public:
    bool process(Renderer::PostProcess::Frame& frame) override
    {
        ++calls;
        width = frame.width;
        height = frame.height;
        saw_color = saw_color || frame.color_texture != nullptr;
        if (calls == 1) first_color = frame.color_texture;
        else color_changed = color_changed || frame.color_texture != first_color;
        return true;
    }

    int calls = 0;
    int width = 0;
    int height = 0;
    bool saw_color = false;
    bool color_changed = false;
    void *first_color = nullptr;
};

class VisualFeatureCase final : public Testing::Case {
public:
    VisualFeatureCase(std::string name, Feature feature) : name_(std::move(name)), feature_(feature) {}

    std::string_view name() const override { return name_; }
    Testing::Kind kind() const override { return Testing::Kind::Visual; }
    std::size_t frameCount() const override { return feature_ == Feature::Everything ? 16u : 8u; }

    bool setup(Testing::Context& context, std::string& error) override
    {
        Models::clearCache();

        switch (feature_) {
            case Feature::DirectionalLight:
            case Feature::DirectionalShadow:
                if (!addBaseScene(context.world, Renderer::LightType::Directional, error)) return false;
                break;
            case Feature::PointLight:
            case Feature::PointShadow:
                if (!addBaseScene(context.world, Renderer::LightType::Point, error)) return false;
                break;
            case Feature::SpotLight:
            case Feature::SpotShadow:
                if (!addBaseScene(context.world, Renderer::LightType::Spot, error)) return false;
                break;
            case Feature::AdvancedMaterials:
                if (!addAdvancedMaterials(context.world, error)) return false;
                break;
            case Feature::Environment:
                addCamera(context.world);
                addEnvironment(context.world);
                if (!addModel(context.world, "Assets/Models/visual-environment.obj", error)) return false;
                break;
            case Feature::LinearFog:
                if (!addDepthScene(context.world, Renderer::FogMode::Linear, error)) return false;
                break;
            case Feature::ExponentialFog:
                if (!addDepthScene(context.world, Renderer::FogMode::Exponential, error)) return false;
                break;
            case Feature::GlobalIllumination:
                if (!addBaseScene(context.world, Renderer::LightType::Point, error)) return false;
                addGlobalIllumination(context.world, false);
                break;
            case Feature::PhotonMapping:
                if (!addBaseScene(context.world, Renderer::LightType::Point, error)) return false;
                addGlobalIllumination(context.world, true);
                break;
            case Feature::GaussianSplat:
                addCamera(context.world);
                addEnvironment(context.world);
                if (!context.graphics) {
                    error = "gaussian visual test has no graphics fixture";
                    return false;
                }
                probe_ = &context.graphics->postProcess().add<ProbePass>();
                break;
            case Feature::PostProcess:
                if (!addBaseScene(context.world, Renderer::LightType::Point, error)) return false;
                if (!context.graphics) {
                    error = "post-process visual test has no graphics fixture";
                    return false;
                }
                probe_ = &context.graphics->postProcess().add<ProbePass>();
                break;
            case Feature::Everything:
                if (!addAdvancedMaterials(context.world, error)) return false;
                if (!addGaussianSplats(context.world, error)) return false;
                addGlobalIllumination(context.world, true);
                if (!context.graphics) {
                    error = "everything visual test has no graphics fixture";
                    return false;
                }
                probe_ = &context.graphics->postProcess().add<ProbePass>();
                break;
        }

        return true;
    }

    bool update(Testing::Context& context, double, std::string& error) override
    {
        if (feature_ == Feature::GaussianSplat && updates_ == 1u &&
            !addGaussianSplats(context.world, error))
            return false;
        ++updates_;
        return true;
    }

    bool verify(Testing::Context& context, std::string& error) override
    {
        if (!Testing::require(context.graphics && context.graphics->manager().active() != nullptr,
                              "visual feature test has no active renderer", error))
            return false;

        if (probe_) {
            if (!Testing::require(probe_->calls > 0, "post-process pass was never invoked", error)) return false;
            if (!Testing::require(probe_->width > 0 && probe_->height > 0, "post-process frame dimensions invalid", error)) return false;
            if (!Testing::require(probe_->saw_color, "post-process pass never received a color target", error)) return false;
            if (feature_ == Feature::GaussianSplat &&
                !Testing::require(probe_->color_changed,
                                  "post-process did not receive gaussian composited color", error))
                return false;
        }

        return true;
    }

    void shutdown(Testing::Context&) override
    {
        if (feature_ == Feature::GlobalIllumination || feature_ == Feature::PhotonMapping || feature_ == Feature::Everything)
            Renderer::GlobalIllumination::reset();
        probe_ = nullptr;
        updates_ = 0u;
    }

private:
    std::string name_;
    Feature feature_;
    ProbePass *probe_ = nullptr;
    std::size_t updates_ = 0u;
};

} // namespace

void registerVisualRendering(Testing::Runner& runner)
{
    runner.add<VisualFeatureCase>("rendering/visual/light-directional", Feature::DirectionalLight);
    runner.add<VisualFeatureCase>("rendering/visual/light-point", Feature::PointLight);
    runner.add<VisualFeatureCase>("rendering/visual/light-spot", Feature::SpotLight);
    runner.add<VisualFeatureCase>("rendering/visual/shadow-directional", Feature::DirectionalShadow);
    runner.add<VisualFeatureCase>("rendering/visual/shadow-point", Feature::PointShadow);
    runner.add<VisualFeatureCase>("rendering/visual/shadow-spot", Feature::SpotShadow);
    runner.add<VisualFeatureCase>("rendering/visual/advanced-materials", Feature::AdvancedMaterials);
    runner.add<VisualFeatureCase>("rendering/visual/environment", Feature::Environment);
    runner.add<VisualFeatureCase>("rendering/visual/fog-linear", Feature::LinearFog);
    runner.add<VisualFeatureCase>("rendering/visual/fog-exponential", Feature::ExponentialFog);
    runner.add<VisualFeatureCase>("rendering/visual/global-illumination", Feature::GlobalIllumination);
    runner.add<VisualFeatureCase>("rendering/visual/photon-mapping", Feature::PhotonMapping);
    runner.add<VisualFeatureCase>("rendering/visual/gaussian-splat", Feature::GaussianSplat);
    runner.add<VisualFeatureCase>("rendering/visual/post-process", Feature::PostProcess);
    runner.add<VisualFeatureCase>("rendering/visual/everything", Feature::Everything);
}

} // namespace Tests
