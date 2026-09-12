#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Tests/Fixtures/RendererFixture.hpp"

#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/Environment.hpp"
#include "Renderer/GlobalIllumination/GlobalIllumination.hpp"
#include "Renderer/PostProcess.hpp"

#include <array>
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

Models::MeshHandle registerQuad()
{
    Models::MeshData mesh;
    mesh.vertices = {
        Models::Vertex{.position = {-1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}},
        Models::Vertex{.position = { 1.0f, -1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}},
        Models::Vertex{.position = { 1.0f,  1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {1.0f, 1.0f}},
        Models::Vertex{.position = {-1.0f,  1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 1.0f}},
    };
    mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    mesh.bounds = {{-1.0f, -1.0f, -0.01f}, {1.0f, 1.0f, 0.01f}};
    return Models::registerMesh(std::move(mesh));
}

Models::MaterialHandle registerMaterial(
    Renderer::Vec3 color,
    float roughness = 0.65f,
    float metallic = 0.0f)
{
    Models::MaterialData material;
    material.name = "visual-regression";
    material.color = color;
    material.roughness = roughness;
    material.metallic = metallic;
    return Models::registerMaterial(std::move(material));
}

Ecs::Entity addRenderable(
    Ecs::World& world,
    Models::MeshHandle mesh,
    Models::MaterialHandle material,
    Renderer::Vec3 position,
    Renderer::Vec3 scale = {1.0f, 1.0f, 1.0f})
{
    const Ecs::Entity entity = world.createEntity();
    world.add<Renderer::Transform>(entity, Renderer::Transform{.position = position, .scale = scale});
    world.add<Renderer::MeshComponent>(entity, Renderer::MeshComponent{mesh, material});
    world.add<Renderer::RenderableComponent>(entity, Renderer::RenderableComponent{true});
    return entity;
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
}

void addBaseScene(Ecs::World& world, Renderer::LightType light)
{
    addCamera(world);
    addEnvironment(world);
    addLight(world, light);

    const Models::MeshHandle quad = registerQuad();
    const Models::MaterialHandle receiver = registerMaterial({0.42f, 0.46f, 0.52f}, 0.85f, 0.0f);
    const Models::MaterialHandle foreground = registerMaterial({0.92f, 0.24f, 0.08f}, 0.38f, 0.15f);

    addRenderable(world, quad, receiver, {0.0f, 0.0f, -5.2f}, {2.7f, 1.75f, 1.0f});
    addRenderable(world, quad, foreground, {-0.45f, 0.15f, -3.45f}, {0.55f, 0.75f, 1.0f});
}

void addDepthScene(Ecs::World& world, Renderer::FogMode fog)
{
    addCamera(world);
    addEnvironment(world, fog);
    addLight(world, Renderer::LightType::Directional);
    const Models::MeshHandle quad = registerQuad();
    const Models::MaterialHandle near_material = registerMaterial({0.9f, 0.18f, 0.08f});
    const Models::MaterialHandle middle_material = registerMaterial({0.1f, 0.7f, 0.25f});
    const Models::MaterialHandle far_material = registerMaterial({0.08f, 0.32f, 0.95f});
    addRenderable(world, quad, near_material, {-1.15f, -0.15f, -3.0f}, {0.55f, 0.75f, 1.0f});
    addRenderable(world, quad, middle_material, {0.0f, 0.0f, -4.8f}, {0.65f, 0.85f, 1.0f});
    addRenderable(world, quad, far_material, {1.2f, 0.15f, -7.2f}, {0.8f, 1.0f, 1.0f});
}

void addAdvancedMaterials(Ecs::World& world)
{
    addCamera(world);
    addEnvironment(world);
    addLight(world, Renderer::LightType::Point);

    const Models::MeshHandle quad = registerQuad();

    Models::MaterialData coated;
    coated.name = "clearcoat-sheen";
    coated.color = {0.72f, 0.08f, 0.04f};
    coated.roughness = 0.22f;
    coated.metallic = 0.15f;
    coated.clearcoat = 1.0f;
    coated.clearcoat_roughness = 0.06f;
    coated.sheen_color = {0.8f, 0.18f, 0.08f};
    coated.sheen_roughness = 0.35f;

    Models::MaterialData film;
    film.name = "iridescent-anisotropic";
    film.color = {0.08f, 0.32f, 0.72f};
    film.roughness = 0.3f;
    film.metallic = 0.55f;
    film.anisotropy_strength = 0.8f;
    film.anisotropy_rotation = 0.55f;
    film.iridescence = 0.9f;
    film.iridescence_ior = 1.45f;
    film.iridescence_thickness_min = 180.0f;
    film.iridescence_thickness_max = 520.0f;

    Models::MaterialData transmissive;
    transmissive.name = "transmission-dispersion";
    transmissive.color = {0.82f, 0.92f, 1.0f};
    transmissive.roughness = 0.08f;
    transmissive.ior = 1.52f;
    transmissive.transmission = 0.72f;
    transmissive.thickness = 0.65f;
    transmissive.attenuation_distance = 2.5f;
    transmissive.attenuation_color = {0.72f, 0.9f, 1.0f};
    transmissive.dispersion = 0.35f;

    const Models::MaterialHandle coated_handle = Models::registerMaterial(std::move(coated));
    const Models::MaterialHandle film_handle = Models::registerMaterial(std::move(film));
    const Models::MaterialHandle transmission_handle = Models::registerMaterial(std::move(transmissive));

    addRenderable(world, quad, coated_handle, {-1.35f, 0.0f, -4.0f}, {0.72f, 0.9f, 1.0f});
    addRenderable(world, quad, film_handle, {0.0f, 0.0f, -4.0f}, {0.72f, 0.9f, 1.0f});
    addRenderable(world, quad, transmission_handle, {1.35f, 0.0f, -4.0f}, {0.72f, 0.9f, 1.0f});
}

Models::AttributeData attribute(std::uint32_t components, std::initializer_list<double> values)
{
    Models::AttributeData result;
    result.component_type = 5126;
    result.components = components;
    result.values.assign(values.begin(), values.end());
    return result;
}

void addGaussianSplats(Ecs::World& world)
{
    Models::MeshData mesh;
    mesh.primitive_mode = Models::PrimitiveMode::Points;
    mesh.vertices = {
        Models::Vertex{.position = {-0.7f, -0.15f, -3.2f}},
        Models::Vertex{.position = { 0.0f,  0.55f, -3.4f}},
        Models::Vertex{.position = { 0.7f, -0.15f, -3.2f}},
    };
    mesh.bounds = {{-1.0f, -0.5f, -3.7f}, {1.0f, 0.9f, -2.9f}};
    mesh.extensions_json["KHR_gaussian_splatting"] =
        R"({"kernel":"ellipse","colorSpace":"srgb_rec709_display","projection":"perspective","sortingMethod":"cameraDistance"})";
    mesh.attributes["KHR_gaussian_splatting:ROTATION"] = attribute(4u, {
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
        0.0, 0.0, 0.0, 1.0,
    });
    mesh.attributes["KHR_gaussian_splatting:SCALE"] = attribute(3u, {
        0.26, 0.18, 0.18,
        0.22, 0.28, 0.18,
        0.26, 0.18, 0.18,
    });
    mesh.attributes["KHR_gaussian_splatting:OPACITY"] = attribute(1u, {0.92, 0.88, 0.92});
    mesh.attributes["KHR_gaussian_splatting:SH_DEGREE_0_COEF_0"] = attribute(3u, {
        1.15, 0.08, 0.04,
        0.05, 1.05, 0.12,
        0.06, 0.18, 1.15,
    });

    Models::MaterialData material;
    material.name = "gaussian-splat";
    material.unlit = true;
    const Models::MeshHandle mesh_handle = Models::registerMesh(std::move(mesh));
    const Models::MaterialHandle material_handle = Models::registerMaterial(std::move(material));
    addRenderable(world, mesh_handle, material_handle, {0.0f, 0.0f, 0.0f});
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
        return true;
    }

    int calls = 0;
    int width = 0;
    int height = 0;
    bool saw_color = false;
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
                addBaseScene(context.world, Renderer::LightType::Directional);
                break;
            case Feature::PointLight:
            case Feature::PointShadow:
                addBaseScene(context.world, Renderer::LightType::Point);
                break;
            case Feature::SpotLight:
            case Feature::SpotShadow:
                addBaseScene(context.world, Renderer::LightType::Spot);
                break;
            case Feature::AdvancedMaterials:
                addAdvancedMaterials(context.world);
                break;
            case Feature::Environment:
                addCamera(context.world);
                addEnvironment(context.world);
                addRenderable(context.world, registerQuad(), registerMaterial({0.42f, 0.45f, 0.5f}),
                              {0.0f, 0.0f, -4.5f}, {1.7f, 1.1f, 1.0f});
                break;
            case Feature::LinearFog:
                addDepthScene(context.world, Renderer::FogMode::Linear);
                break;
            case Feature::ExponentialFog:
                addDepthScene(context.world, Renderer::FogMode::Exponential);
                break;
            case Feature::GlobalIllumination:
                addBaseScene(context.world, Renderer::LightType::Point);
                addGlobalIllumination(context.world, false);
                break;
            case Feature::PhotonMapping:
                addBaseScene(context.world, Renderer::LightType::Point);
                addGlobalIllumination(context.world, true);
                break;
            case Feature::GaussianSplat:
                addCamera(context.world);
                addEnvironment(context.world);
                addGaussianSplats(context.world);
                break;
            case Feature::PostProcess:
                addBaseScene(context.world, Renderer::LightType::Point);
                if (!context.graphics) {
                    error = "post-process visual test has no graphics fixture";
                    return false;
                }
                probe_ = &context.graphics->postProcess().add<ProbePass>();
                break;
            case Feature::Everything:
                addAdvancedMaterials(context.world);
                addGaussianSplats(context.world);
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

    bool verify(Testing::Context& context, std::string& error) override
    {
        if (!Testing::require(context.graphics && context.graphics->manager().active() != nullptr,
                              "visual feature test has no active renderer", error))
            return false;

        if (probe_) {
            if (!Testing::require(probe_->calls > 0, "post-process pass was never invoked", error)) return false;
            if (!Testing::require(probe_->width > 0 && probe_->height > 0, "post-process frame dimensions invalid", error)) return false;
            if (!Testing::require(probe_->saw_color, "post-process pass never received a color target", error)) return false;
        }

        return true;
    }

    void shutdown(Testing::Context&) override
    {
        if (feature_ == Feature::GlobalIllumination || feature_ == Feature::PhotonMapping || feature_ == Feature::Everything)
            Renderer::GlobalIllumination::reset();
        probe_ = nullptr;
    }

private:
    std::string name_;
    Feature feature_;
    ProbePass *probe_ = nullptr;
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
