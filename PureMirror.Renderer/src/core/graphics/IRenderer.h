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
    virtual void ReleaseTexture(const std::string& path) = 0;
    virtual void CleanUnusedTextures(const std::unordered_set<ImTextureID>& usedTextures) = 0;
    [[nodiscard]] virtual RendererType GetType() noexcept = 0;
    virtual void Reset() = 0;
    virtual void SetRenderThread(RenderThread* renderThread) = 0;
};
