#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "TextureAsset.h"

#include <include/Texture.h>

class IRenderer
{
  public:
    virtual ~IRenderer() = default;

    virtual Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) = 0;

    virtual void ReleaseTexture(const std::string& path) = 0;
};
