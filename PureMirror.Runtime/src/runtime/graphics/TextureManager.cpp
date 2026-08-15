#include "pch.h"

#include "TextureManager.h"

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

std::shared_ptr<TextureAsset> TextureManager::Load(const std::string& path)
{
    const std::string normalizedPath = NormalizePath(path);

    if (auto existing = Get(normalizedPath))
        return existing;

    auto texture = LoadFromDisk(normalizedPath);

    if (!texture)
        return nullptr;

    m_Textures.emplace(normalizedPath, texture);

    return texture;
}

std::shared_ptr<TextureAsset> TextureManager::Get(const std::string& path) const noexcept
{
    auto it = m_Textures.find(path);

    if (it == m_Textures.end())
        return nullptr;

    return it->second;
}

std::shared_ptr<TextureAsset> TextureManager::ReloadIfChanged(const std::string& path)
{
    const std::string normalizedPath = NormalizePath(path);

    auto existing = Get(normalizedPath);

    if (!existing)
        return Load(normalizedPath);

    std::error_code ec;

    const auto currentWriteTime = std::filesystem::last_write_time(normalizedPath, ec);

    if (ec)
        return existing;

    if (currentWriteTime == existing->LastWriteTime)
        return existing;

    auto reloaded = LoadFromDisk(normalizedPath);

    if (!reloaded)
        return existing;

    m_Textures.insert_or_assign(normalizedPath, reloaded);

    return reloaded;
}

std::shared_ptr<TextureAsset> TextureManager::LoadFromDisk(const std::string& path)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
        return nullptr;

    auto texture = std::make_shared<TextureAsset>();
    texture->Path = path;
    texture->Width = width;
    texture->Height = height;
    texture->Channels = channels;

    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    texture->Pixels.resize(size);

    std::memcpy(texture->Pixels.data(), pixels, size);

    stbi_image_free(pixels);

    std::error_code ec;

    texture->LastWriteTime = std::filesystem::last_write_time(path, ec);

    return texture;
}

std::string TextureManager::NormalizePath(const std::string& path)
{
    return std::filesystem::weakly_canonical(path).string();
}
