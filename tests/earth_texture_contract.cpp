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
    if (asset->image.width != 8192 || asset->image.height != 4096) return 4;
    if (asset->image.rgba.size() != static_cast<std::size_t>(8192ull * 4096ull * 4ull)) return 5;

    Models::clearTextureCache();
    return 0;
}
