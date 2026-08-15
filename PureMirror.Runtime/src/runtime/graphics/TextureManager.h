#pragma once

#include "TextureAsset.h"

class TextureManager
{
  public:
    [[nodiscard]] std::shared_ptr<TextureAsset> Load(const std::string& path);
    [[nodiscard]] std::shared_ptr<TextureAsset> Get(const std::string& path) const noexcept;
    [[nodiscard]] std::shared_ptr<TextureAsset> ReloadIfChanged(const std::string& path);

  private:
    [[nodiscard]] std::shared_ptr<TextureAsset> LoadFromDisk(const std::string& path);
    [[nodiscard]] std::string NormalizePath(const std::string& path);
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_Textures;
};
