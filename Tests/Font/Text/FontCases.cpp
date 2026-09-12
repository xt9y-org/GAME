#include "Tests/Harness/Testing.hpp"
#include "Tests/Register.hpp"

#include "Font.hpp"

namespace Tests {
namespace {

class AtlasCase final : public Testing::Case {
public:
    std::string_view name() const override { return "font/atlas"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Font::configureAtlas("Assets/Font/font.png", 16u, 16u, 8.0f);
        const Font::AtlasSettings& atlas = Font::atlas();
        return Testing::require(atlas.path == "Assets/Font/font.png", "font path mismatch", error) &&
            Testing::require(atlas.columns == 16u && atlas.rows == 16u, "font grid mismatch", error) &&
            Testing::require(Testing::near(atlas.screen_cell_pixels, 8.0f), "font cell size mismatch", error);
    }
};

class ScreenCase final : public Testing::Case {
public:
    std::string_view name() const override { return "font/screen"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = Font::screen(world, "screen", {12.0f, 18.0f}, 2.0f, {1.0f, 0.5f, 0.25f, 1.0f});
        const Font::TextComponent *text = world.get<Font::TextComponent>(entity);
        return Testing::require(text && text->space == Font::Space::Screen, "screen text component missing", error) &&
            Testing::require(text->text == "screen" && Testing::near(text->scale, 2.0f), "screen text values mismatch", error);
    }
};

class WorldCase final : public Testing::Case {
public:
    std::string_view name() const override { return "font/world"; }
    bool verify(Testing::Context&, std::string& error) override
    {
        Ecs::World world;
        const Ecs::Entity entity = Font::world(world, "world", Renderer::Transform{.position = {1.0f, 2.0f, 3.0f}}, 0.5f,
                                               {0.5f, 1.0f, 0.5f, 1.0f}, false);
        const Font::TextComponent *text = world.get<Font::TextComponent>(entity);
        const Renderer::Transform *transform = world.get<Renderer::Transform>(entity);
        return Testing::require(text && transform, "world text components missing", error) &&
            Testing::require(text->space == Font::Space::World && !text->depth_test, "world text flags mismatch", error) &&
            Testing::require(Testing::near(transform->position.x, 1.0f) && Testing::near(transform->position.y, 2.0f),
                             "world text transform mismatch", error);
    }
};

} // namespace

void registerFont(Testing::Runner& runner)
{
    runner.add<AtlasCase>();
    runner.add<ScreenCase>();
    runner.add<WorldCase>();
}

} // namespace Tests
