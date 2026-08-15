#pragma once

#include "pch.h"

#include "../TextureAsset.h"
#include "VulkanTexture.h"

#include <vulkan/vulkan.h>

class VulkanGpuUploader
{
  public:
    VulkanGpuUploader(VkPhysicalDevice physicalDevice,
                      VkDevice device,
                      VkQueue queue,
                      uint32_t queueFamily,
                      VkDescriptorPool descriptorPool,
                      const VkAllocationCallbacks* allocator = nullptr);

    ~VulkanGpuUploader();

    VulkanGpuUploader(const VulkanGpuUploader&) = delete;
    VulkanGpuUploader& operator=(const VulkanGpuUploader&) = delete;
    VulkanGpuUploader(VulkanGpuUploader&&) = delete;
    VulkanGpuUploader& operator=(VulkanGpuUploader&&) = delete;

    [[nodiscard]] std::shared_ptr<VulkanTexture> UploadTexture(std::shared_ptr<TextureAsset> asset);
    void ReleaseTexture(std::shared_ptr<VulkanTexture> texture);

    // Must run on the thread which owns the graphics queue.
    void ProcessPendingTextures();

    [[nodiscard]] bool IsReady(const std::shared_ptr<VulkanTexture>& texture) const noexcept;
    [[nodiscard]] uint64_t GetAllocatedBytes() const noexcept;
    [[nodiscard]] uint64_t GetDeviceLocalMemoryBudget() const noexcept;

  private:
    struct UploadRequest
    {
        std::shared_ptr<TextureAsset> Asset;
        std::shared_ptr<VulkanTexture> Texture;
    };

    void Shutdown();
    void UploadTextureInternal(const UploadRequest& request);
    void ReleaseTextureInternal(const std::shared_ptr<VulkanTexture>& texture);

    [[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

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

    std::atomic<uint64_t> m_AllocatedBytes = 0;
};
