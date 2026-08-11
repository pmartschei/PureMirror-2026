#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "RenderThread.h"
#include "RendererType.h"
#include "TextureAsset.h"

#include <include/Texture.h>

class IRenderer
{
  public:
    virtual ~IRenderer() = default;

    [[nodiscard]] virtual Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) = 0;

    // TODO Should probably add a strategy on how textures should be deleted, like when no memory limit is reached, or
    // every minute all older than 30 seconds textures
    virtual void CleanUpTextures() = 0;
    virtual void MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) = 0;
    [[nodiscard]] virtual RendererType GetType() noexcept = 0;
    virtual void Reset() = 0;
    virtual void SetRenderThread(RenderThread* renderThread) = 0;
};
