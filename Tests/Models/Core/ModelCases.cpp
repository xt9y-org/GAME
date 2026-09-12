#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Models/Compression/Checksums.hpp"
#include "Models/Compression/Deflate.hpp"
#include "Models/Core/Material.hpp"
#include "Models/Formats/Registry.hpp"
#include "Models/Images/Registry.hpp"
#include "Models/Models.hpp"
#include "Renderer/Components.hpp"
#include "Renderer/ModelScene.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace Tests {
namespace {

bool loadSceneFormat(const char *path, std::string_view label, std::string& error)
{
    Models::clearCache();
    std::string load_error;
    const Models::ModelHandle model = Models::load(path, &load_error);
    if (model == Models::INVALID_MODEL) {
        error = std::string(label) + " load failed: " + load_error;
        return false;
    }
    return Testing::require(Models::nodeCount(model) == 1u, std::string(label) + " node count mismatch", error) &&
        Testing::require(Models::sceneCount(model) == 1u, std::string(label) + " scene count mismatch", error) &&
        Testing::require(Models::defaultScene(model) == 0u, std::string(label) + " default scene mismatch", error);
}

bool loadMeshFormat(const char *path, std::string_view label, std::string& error)
{
    Models::clearCache();
    std::string load_error;
    const Models::ModelHandle model = Models::load(path, &load_error);
    if (model == Models::INVALID_MODEL) {
        error = std::string(label) + " load failed: " + load_error;
        return false;
    }
    if (!Testing::require(Models::partCount(model) > 0u, std::string(label) + " produced no model parts", error)) return false;
    const Models::ModelPart *part = Models::part(model, 0u);
    if (!Testing::require(part != nullptr && part->mesh != Models::INVALID_MESH,
                          std::string(label) + " first mesh part is invalid", error)) return false;
    const Models::MeshData *mesh = Models::mesh(part->mesh);
    return Testing::require(mesh != nullptr && mesh->vertices.size() >= 3u && mesh->indices.size() >= 3u,
                            std::string(label) + " triangle geometry was not decoded", error);
}

class FormatsCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/formats"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        using Models::Formats::loaderFor;
        return Testing::require(loaderFor(".gltf") != nullptr, "glTF loader missing", error) &&
            Testing::require(loaderFor(".glb") != nullptr, "GLB loader missing", error) &&
            Testing::require(loaderFor(".fbx") != nullptr, "FBX loader missing", error) &&
            Testing::require(loaderFor(".obj") != nullptr, "OBJ loader missing", error) &&
            loadSceneFormat("Assets/Models/minimal.gltf", "glTF", error) &&
            loadSceneFormat("Assets/Models/minimal.glb", "GLB", error) &&
            loadMeshFormat("Assets/Models/minimal.obj", "OBJ", error) &&
            loadMeshFormat("Assets/Models/minimal.fbx", "FBX", error);
    }
};

class GltfLoaderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/formats/gltf"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        return loadSceneFormat("Assets/Models/minimal.gltf", "glTF", error);
    }
};

class GlbLoaderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/formats/glb"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        return loadSceneFormat("Assets/Models/minimal.glb", "GLB", error);
    }
};

class ObjLoaderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/formats/obj"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        return loadMeshFormat("Assets/Models/minimal.obj", "OBJ", error);
    }
};

class FbxLoaderCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/formats/fbx"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        return loadMeshFormat("Assets/Models/minimal.fbx", "FBX", error);
    }
};

class ImagesCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/images"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        using Models::Images::decoderFor;
        return Testing::require(decoderFor(".png") != nullptr, "PNG decoder missing", error) &&
            Testing::require(decoderFor(".jpg") != nullptr || decoderFor(".jpeg") != nullptr, "JPEG decoder missing", error) &&
            Testing::require(decoderFor(".tga") != nullptr, "TGA decoder missing", error) &&
            Testing::require(decoderFor(".webp") != nullptr, "WebP decoder missing", error) &&
            Testing::require(decoderFor(".ktx2") != nullptr, "KTX2 decoder missing", error);
    }
};

class CompressionCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/compression"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        constexpr std::array<std::uint8_t, 9> source{{'1','2','3','4','5','6','7','8','9'}};
        if (!Testing::require(Models::Compression::crc32(source.data(), source.size()) == 0xcbf43926u,
                              "CRC32 mismatch", error)) return false;
        if (!Testing::require(Models::Compression::adler32(source.data(), source.size()) == 0x091e01deu,
                              "Adler32 mismatch", error)) return false;

        constexpr std::array<std::uint8_t, 13> compressed{{120,218,243,200,47,42,78,5,0,5,202,2,2}};
        std::vector<std::uint8_t> output;
        std::string inflate_error;
        if (!Models::Compression::inflateZlib(compressed.data(), compressed.size(), &output, &inflate_error)) {
            error = inflate_error;
            return false;
        }
        const std::array<std::uint8_t, 5> expected{{'H','o','r','s','e'}};
        return Testing::require(output.size() == expected.size() &&
                                std::equal(output.begin(), output.end(), expected.begin()),
                                "zlib inflate mismatch", error);
    }
};

class MaterialsCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/materials"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Models::MaterialData material;
        material.name = "advanced";
        material.color = {0.2f, 0.4f, 0.6f};
        material.roughness = 0.25f;
        material.metallic = 0.75f;
        material.ior = 1.45f;
        material.clearcoat = 0.8f;
        material.sheen_color = {0.1f, 0.2f, 0.3f};
        material.transmission = 0.5f;
        material.thickness = 0.2f;
        material.specular = 0.7f;
        material.iridescence = 0.4f;
        material.anisotropy_strength = 0.3f;
        material.dispersion = 0.1f;
        material.diffuse_transmission = 0.35f;
        const Models::MaterialHandle handle = Models::registerMaterial(material);
        const Models::MaterialData *stored = Models::material(handle);
        if (!Testing::require(stored != nullptr, "material registration failed", error)) return false;
        if (!Testing::require(Testing::near(stored->clearcoat, 0.8f) && Testing::near(stored->transmission, 0.5f) &&
                              Testing::near(stored->iridescence, 0.4f) && Testing::near(stored->diffuse_transmission, 0.35f),
                              "advanced material fields lost", error)) return false;
        material.clearcoat = 0.1f;
        return Testing::require(Models::updateMaterial(handle, material) && Testing::near(Models::material(handle)->clearcoat, 0.1f),
                                "material update failed", error);
    }
};

class ModelSceneCase final : public Testing::Case {
public:
    std::string_view name() const override { return "models/runtime-scene"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Models::clearCache();
        std::string load_error;
        const Models::ModelHandle model = Models::load("Assets/Interactivity/basic.gltf", &load_error);
        if (model == Models::INVALID_MODEL) { error = load_error; return false; }
        Ecs::World world;
        Renderer::ModelScene::Instance instance;
        if (!Renderer::ModelScene::instantiate(world, model, &instance, {}, &load_error)) {
            error = load_error;
            return false;
        }
        if (!Testing::require(instance.nodes.size() == 1u, "model scene node count mismatch", error)) return false;
        const Ecs::Entity entity = instance.nodes.front().entity;
        if (!Testing::require(world.has<Renderer::Transform>(entity), "model scene transform missing", error)) return false;
        Renderer::ModelScene::destroy(world, instance);
        return Testing::require(instance.model == Models::INVALID_MODEL && !world.alive(entity),
                                "model scene destruction leaked entity/state", error);
    }
};

} // namespace

void registerModels(Testing::Runner& runner)
{
    runner.add<FormatsCase>();
    runner.add<GltfLoaderCase>();
    runner.add<GlbLoaderCase>();
    runner.add<ObjLoaderCase>();
    runner.add<FbxLoaderCase>();
    runner.add<ImagesCase>();
    runner.add<CompressionCase>();
    runner.add<MaterialsCase>();
    runner.add<ModelSceneCase>();
}

} // namespace Tests
