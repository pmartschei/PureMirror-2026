#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include "../IRenderer.h"

#include <vulkan/vulkan.h>

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

    // Must run on the thread which owns the graphics queue. Upload requests are
    // produced by the ImGui render thread and consumed immediately before draw.
    void ProcessPendingTextures();

    void Shutdown();

    [[nodiscard]] RenderThread* GetRenderThread() const noexcept
    {
        return m_RenderThread;
    }

  private:
    enum class TextureState : uint8_t
    {
        Pending,
        Ready,
        Failed,
        Released
    };

    struct VulkanTexture
    {
        VkImage Image = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
        VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;

        uint32_t Width = 0;
        uint32_t Height = 0;
        uint64_t SizeInBytes = 0;
        uint64_t LastUsedFrame = 0;

        std::atomic<TextureState> State{TextureState::Pending};
    };

    struct UploadRequest
    {
        std::shared_ptr<TextureAsset> Asset;
        std::shared_ptr<VulkanTexture> Texture;
    };

    static constexpr double CLEANUP_THRESHOLD = 0.90;
    static constexpr double CLEANUP_THRESHOLD_DESIRED = 0.60;

    RenderThread* m_RenderThread = nullptr;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_Queue = VK_NULL_HANDLE;
    uint32_t m_QueueFamily = UINT32_MAX;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    const VkAllocationCallbacks* m_Allocator = nullptr;

    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    std::mutex m_Mutex;
    std::queue<UploadRequest> m_UploadQueue;
    std::queue<std::shared_ptr<VulkanTexture>> m_ReleaseQueue;
    std::unordered_map<std::shared_ptr<TextureAsset>, std::shared_ptr<VulkanTexture>> m_Textures;

    std::atomic<uint64_t> m_AllocatedBytes = 0;

    // Inherited via IRenderer
    virtual [[nodiscard]] Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
    virtual void CleanUpTextures() override;
    virtual void MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) override;
    virtual [[nodiscard]] RendererType GetType() noexcept override;
    virtual void Reset() override;
    virtual void SetRenderThread(RenderThread* renderThread) override;

    void UploadTexture(const UploadRequest& request);
    void ReleaseTexture(const std::shared_ptr<VulkanTexture>& texture);
    void CleanUpFailedTextures();
    void CleanUpUnusedTextures(uint64_t budget, uint64_t currentUsage);

    [[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] uint64_t GetDeviceLocalMemoryBudget() const;
};
