#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include "../IRenderer.h"

class VulkanRenderer : public IRenderer
{
  public:
    VulkanRenderer();

    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    VulkanRenderer(const VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&&) = delete;

    [[nodiscard]] RenderThread* GetRenderThread() const noexcept
    {
        return m_RenderThread;
    }

  private:
    RenderThread* m_RenderThread;
    // Inherited via IRenderer
    virtual [[nodiscard]] Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
    virtual void CleanUpTextures() override;
    virtual void MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) override;
    virtual [[nodiscard]] RendererType GetType() noexcept override;
    virtual void Reset() override;
    virtual void SetRenderThread(RenderThread* renderThread) override;
};
