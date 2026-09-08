#include "Sources/Models/Core/Texture.hpp"

#include <string>

int main()
{
    std::string error;
    const Models::TextureHandle texture = Models::loadTexture(
        "Assets/Textures/earth_diffuse.jpg",
        &error
    );
    if (texture == Models::INVALID_TEXTURE) return 2;

    const Models::TextureAsset *asset = Models::texture(texture);
    if (!asset) return 3;
    if (asset->image.width != 2048 || asset->image.height != 1024) return 4;
    if (asset->image.rgba.size() != static_cast<std::size_t>(2048 * 1024 * 4)) return 5;

    Models::clearTextureCache();
    return 0;
}
