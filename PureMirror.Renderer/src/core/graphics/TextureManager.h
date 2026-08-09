#pragma once

#include "TextureAsset.h"

class TextureManager
{
  public:
    std::shared_ptr<TextureAsset> Load(const std::string& path);
    std::shared_ptr<TextureAsset> Get(const std::string& path) const;
    void Unload(const std::string& path);

  private:
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_Textures;
};
