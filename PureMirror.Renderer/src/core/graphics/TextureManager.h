#pragma once

#include "TextureAsset.h"

class TextureManager
{
  public:
    [[nodiscard]] std::shared_ptr<TextureAsset> Load(const std::string& path);
    [[nodiscard]] std::shared_ptr<TextureAsset> Get(const std::string& path) const noexcept;
    void Unload(const std::string& path) noexcept;

  private:
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_Textures;
};
