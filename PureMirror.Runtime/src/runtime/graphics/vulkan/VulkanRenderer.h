#pragma once
#include "pch.h"

#include "../IRenderer.h"
#include "VulkanGpuUploader.h"

class VulkanRenderer : public IRenderer
{
  public:
    VulkanRenderer();
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

    void Initialize(VkPhysicalDevice physicalDevice,
                    VkDevice device,
                    VkQueue queue,
                    uint32_t queueFamily,
                    VkDescriptorPool descriptorPool,
                    const VkAllocationCallbacks* allocator = nullptr);

    void ProcessPendingTextures();
    void Shutdown();

    [[nodiscard]] RenderThread* GetRenderThread() const noexcept
    {
        return m_RenderThread;
    }

  private:
    static constexpr double CLEANUP_THRESHOLD = 0.90;
    static constexpr double CLEANUP_THRESHOLD_DESIRED = 0.60;

    RenderThread* m_RenderThread = nullptr;
    std::unique_ptr<VulkanGpuUploader> m_GpuUploader;
    std::unordered_map<std::shared_ptr<TextureAsset>, std::shared_ptr<VulkanTexture>> m_Textures;

    // Inherited via IRenderer
    virtual [[nodiscard]] Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
    virtual void CleanUpTextures() override;
    virtual void MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) override;
    virtual [[nodiscard]] RendererType GetType() noexcept override;
    virtual void Reset() override;
    virtual void SetRenderThread(RenderThread* renderThread) override;

    void CleanUpFailedTextures();
    void CleanUpUnusedTextures(uint64_t budget, uint64_t currentUsage);
};
