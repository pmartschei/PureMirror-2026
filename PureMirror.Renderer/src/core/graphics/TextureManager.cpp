// clang-format off
#include "pch.h"
// clang-format on

#include "TextureManager.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

std::shared_ptr<TextureAsset> TextureManager::Load(const std::string& path)
{
    if (auto existing = Get(path))
        return existing;

    auto texture = std::make_shared<TextureAsset>();
    texture->Path = path;

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
        return nullptr;

    texture->Width = width;
    texture->Height = height;
    texture->Channels = channels;

    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    texture->Pixels.resize(size);

    std::memcpy(texture->Pixels.data(), pixels, size);

    stbi_image_free(pixels);

    m_Textures.emplace(path, texture);

    return texture;
}

std::shared_ptr<TextureAsset> TextureManager::Get(const std::string& path) const noexcept
{
    auto it = m_Textures.find(path);

    if (it == m_Textures.end())
        return nullptr;

    return it->second;
}

void TextureManager::Unload(const std::string& path) noexcept
{
    m_Textures.erase(path);
}
