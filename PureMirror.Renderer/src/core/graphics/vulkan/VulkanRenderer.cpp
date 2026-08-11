// clang-format off
#include "pch.h"
// clang-format on

#include "VulkanRenderer.h"

#include <algorithm>
#include <external/imgui/imgui_impl_vulkan.h>
#include <hooks/backend/vulkan/hook_vulkan.h>
#include <ranges>
#include <stdexcept>
#include <vector>

VulkanRenderer::VulkanRenderer() {}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

namespace
{
    void CheckVk(VkResult result, const char* message)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(message);
    }
}  // namespace

void VulkanRenderer::Initialize(VkPhysicalDevice physicalDevice,
                                VkDevice device,
                                VkQueue queue,
                                uint32_t queueFamily,
                                VkDescriptorPool descriptorPool,
                                const VkAllocationCallbacks* allocator)
{
    if (m_Device == device && m_Queue == queue && m_CommandPool != VK_NULL_HANDLE)
        return;

    Shutdown();

    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || queue == VK_NULL_HANDLE ||
        queueFamily == UINT32_MAX || descriptorPool == VK_NULL_HANDLE)
    {
        throw std::invalid_argument("VulkanRenderer: invalid initialization handles");
    }

    m_PhysicalDevice = physicalDevice;
    m_Device = device;
    m_Queue = queue;
    m_QueueFamily = queueFamily;
    m_DescriptorPool = descriptorPool;
    m_Allocator = allocator;

    try
    {
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamily;
        CheckVk(vkCreateCommandPool(m_Device, &commandPoolInfo, m_Allocator, &m_CommandPool),
                "VulkanRenderer: could not create upload command pool");

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        CheckVk(vkCreateSampler(m_Device, &samplerInfo, m_Allocator, &m_Sampler),
                "VulkanRenderer: could not create texture sampler");
    }
    catch (...)
    {
        Shutdown();
        throw;
    }
}

void VulkanRenderer::ProcessPendingTextures()
{
    if (m_Device == VK_NULL_HANDLE || m_CommandPool == VK_NULL_HANDLE)
        return;

    std::queue<UploadRequest> uploads;
    std::queue<std::shared_ptr<VulkanTexture>> releases;

    {
        std::lock_guard lock(m_Mutex);
        std::swap(uploads, m_UploadQueue);
        std::swap(releases, m_ReleaseQueue);
    }

    // Releases are rare (memory pressure or shutdown). Waiting here guarantees
    // that a descriptor/image is no longer referenced by an in-flight frame.
    if (!releases.empty())
    {
        vkDeviceWaitIdle(m_Device);
        while (!releases.empty())
        {
            ReleaseTexture(releases.front());
            releases.pop();
        }
    }

    while (!uploads.empty())
    {
        const UploadRequest request = std::move(uploads.front());
        uploads.pop();

        try
        {
            UploadTexture(request);
        }
        catch (...)
        {
            ReleaseTexture(request.Texture);
            request.Texture->State.store(TextureState::Failed, std::memory_order_release);
        }
    }
}

void VulkanRenderer::Shutdown()
{
    if (m_Device == VK_NULL_HANDLE)
        return;

    vkDeviceWaitIdle(m_Device);

    std::unordered_map<std::shared_ptr<TextureAsset>, std::shared_ptr<VulkanTexture>> textures;
    std::queue<UploadRequest> uploads;
    std::queue<std::shared_ptr<VulkanTexture>> releases;

    {
        std::lock_guard lock(m_Mutex);
        textures.swap(m_Textures);
        std::swap(uploads, m_UploadQueue);
        std::swap(releases, m_ReleaseQueue);
    }

    for (auto& [asset, texture] : textures)
        ReleaseTexture(texture);

    while (!releases.empty())
    {
        ReleaseTexture(releases.front());
        releases.pop();
    }

    if (m_Sampler != VK_NULL_HANDLE)
        vkDestroySampler(m_Device, m_Sampler, m_Allocator);
    if (m_CommandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(m_Device, m_CommandPool, m_Allocator);

    m_Sampler = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_DescriptorPool = VK_NULL_HANDLE;
    m_Queue = VK_NULL_HANDLE;
    m_QueueFamily = UINT32_MAX;
    m_Device = VK_NULL_HANDLE;
    m_PhysicalDevice = VK_NULL_HANDLE;
    m_Allocator = nullptr;
    m_AllocatedBytes.store(0, std::memory_order_relaxed);
}

Texture VulkanRenderer::UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset)
{
    if (!asset || asset->Pixels.empty() || asset->Width == 0 || asset->Height == 0)
        return {};

    std::shared_ptr<VulkanTexture> texture;

    {
        std::lock_guard lock(m_Mutex);

        if (const auto it = m_Textures.find(asset); it != m_Textures.end())
        {
            texture = it->second;
        }
        else
        {
            texture = std::make_shared<VulkanTexture>();
            texture->Width = asset->Width;
            texture->Height = asset->Height;
            m_Textures.emplace(asset, texture);
            m_UploadQueue.push({asset, texture});
        }
    }

    Texture result{};
    result.Size = ImVec2(static_cast<float>(texture->Width), static_cast<float>(texture->Height));

    if (texture->State.load(std::memory_order_acquire) == TextureState::Ready)
        result.TextureID = reinterpret_cast<ImTextureID>(texture->DescriptorSet);

    return result;
}

void VulkanRenderer::CleanUpTextures()
{
    CleanUpFailedTextures();

    const uint64_t budget = GetDeviceLocalMemoryBudget();
    const uint64_t usage = m_AllocatedBytes.load(std::memory_order_relaxed);

    if (budget == 0 || static_cast<double>(usage) / static_cast<double>(budget) < CLEANUP_THRESHOLD)
        return;

    CleanUpUnusedTextures(budget, usage);
}

void VulkanRenderer::MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures)
{
    const uint64_t currentFrame = ImGui::GetFrameCount();
    std::lock_guard lock(m_Mutex);

    for (auto& [asset, texture] : m_Textures)
    {
        if (texture && texture->State.load(std::memory_order_acquire) == TextureState::Ready &&
            usedTextures.contains(reinterpret_cast<ImTextureID>(texture->DescriptorSet)))
        {
            texture->LastUsedFrame = currentFrame;
        }
    }
}

RendererType VulkanRenderer::GetType() noexcept
{
    return RendererType::Vulkan;
}

void VulkanRenderer::Reset()
{
    Shutdown();
    SetRenderThread(nullptr);
    VK::Unhook();
}

void VulkanRenderer::SetRenderThread(RenderThread* renderThread)
{
    m_RenderThread = renderThread;
}

void VulkanRenderer::UploadTexture(const UploadRequest& request)
{
    const TextureAsset& asset = *request.Asset;
    VulkanTexture& texture = *request.Texture;
    const VkDeviceSize pixelBytes = static_cast<VkDeviceSize>(asset.Width) * asset.Height * 4;

    if (asset.Pixels.size() < pixelBytes)
        throw std::runtime_error("VulkanRenderer: texture pixel buffer is too small");

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {asset.Width, asset.Height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CheckVk(vkCreateImage(m_Device, &imageInfo, m_Allocator, &texture.Image),
            "VulkanRenderer: could not create texture image");

    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(m_Device, texture.Image, &imageRequirements);

    VkMemoryAllocateInfo imageAllocation{};
    imageAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocation.allocationSize = imageRequirements.size;
    imageAllocation.memoryTypeIndex =
        FindMemoryType(imageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVk(vkAllocateMemory(m_Device, &imageAllocation, m_Allocator, &texture.Memory),
            "VulkanRenderer: could not allocate texture memory");
    CheckVk(vkBindImageMemory(m_Device, texture.Image, texture.Memory, 0),
            "VulkanRenderer: could not bind texture memory");
    texture.SizeInBytes = imageRequirements.size;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    try
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = pixelBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CheckVk(vkCreateBuffer(m_Device, &bufferInfo, m_Allocator, &stagingBuffer),
                "VulkanRenderer: could not create staging buffer");

        VkMemoryRequirements stagingRequirements{};
        vkGetBufferMemoryRequirements(m_Device, stagingBuffer, &stagingRequirements);

        VkMemoryAllocateInfo stagingAllocation{};
        stagingAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stagingAllocation.allocationSize = stagingRequirements.size;
        stagingAllocation.memoryTypeIndex =
            FindMemoryType(stagingRequirements.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        CheckVk(vkAllocateMemory(m_Device, &stagingAllocation, m_Allocator, &stagingMemory),
                "VulkanRenderer: could not allocate staging memory");
        CheckVk(vkBindBufferMemory(m_Device, stagingBuffer, stagingMemory, 0),
                "VulkanRenderer: could not bind staging memory");

        void* mapped = nullptr;
        CheckVk(vkMapMemory(m_Device, stagingMemory, 0, pixelBytes, 0, &mapped),
                "VulkanRenderer: could not map staging memory");
        std::memcpy(mapped, asset.Pixels.data(), static_cast<size_t>(pixelBytes));
        vkUnmapMemory(m_Device, stagingMemory);

        VkCommandBufferAllocateInfo commandAllocation{};
        commandAllocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAllocation.commandPool = m_CommandPool;
        commandAllocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandAllocation.commandBufferCount = 1;
        CheckVk(vkAllocateCommandBuffers(m_Device, &commandAllocation, &commandBuffer),
                "VulkanRenderer: could not allocate upload command buffer");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo),
                "VulkanRenderer: could not begin upload command buffer");

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = texture.Image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &toTransfer);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.imageExtent = {asset.Width, asset.Height, 1};
        vkCmdCopyBufferToImage(
            commandBuffer, stagingBuffer, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier toShaderRead = toTransfer;
        toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &toShaderRead);
        CheckVk(vkEndCommandBuffer(commandBuffer), "VulkanRenderer: could not end upload command buffer");

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        CheckVk(vkCreateFence(m_Device, &fenceInfo, m_Allocator, &fence),
                "VulkanRenderer: could not create upload fence");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        CheckVk(vkQueueSubmit(m_Queue, 1, &submitInfo, fence), "VulkanRenderer: texture upload failed");
        CheckVk(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX),
                "VulkanRenderer: texture upload wait failed");
    }
    catch (...)
    {
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(m_Device, fence, m_Allocator);
        if (commandBuffer != VK_NULL_HANDLE)
            vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
        if (stagingBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(m_Device, stagingBuffer, m_Allocator);
        if (stagingMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_Device, stagingMemory, m_Allocator);
        throw;
    }

    vkDestroyFence(m_Device, fence, m_Allocator);
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
    vkDestroyBuffer(m_Device, stagingBuffer, m_Allocator);
    vkFreeMemory(m_Device, stagingMemory, m_Allocator);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_Device, &viewInfo, m_Allocator, &texture.View),
            "VulkanRenderer: could not create texture view");

    texture.DescriptorSet =
        ImGui_ImplVulkan_AddTexture(m_Sampler, texture.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (texture.DescriptorSet == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanRenderer: could not allocate texture descriptor");

    m_AllocatedBytes.fetch_add(texture.SizeInBytes, std::memory_order_relaxed);
    texture.State.store(TextureState::Ready, std::memory_order_release);
}

void VulkanRenderer::ReleaseTexture(const std::shared_ptr<VulkanTexture>& texture)
{
    if (!texture || m_Device == VK_NULL_HANDLE)
        return;

    if (texture->DescriptorSet != VK_NULL_HANDLE && m_DescriptorPool != VK_NULL_HANDLE)
        vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &texture->DescriptorSet);
    if (texture->View != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device, texture->View, m_Allocator);
    if (texture->Image != VK_NULL_HANDLE)
        vkDestroyImage(m_Device, texture->Image, m_Allocator);
    if (texture->Memory != VK_NULL_HANDLE)
        vkFreeMemory(m_Device, texture->Memory, m_Allocator);

    if (texture->State.load(std::memory_order_acquire) == TextureState::Ready)
        m_AllocatedBytes.fetch_sub(texture->SizeInBytes, std::memory_order_relaxed);

    texture->DescriptorSet = VK_NULL_HANDLE;
    texture->View = VK_NULL_HANDLE;
    texture->Image = VK_NULL_HANDLE;
    texture->Memory = VK_NULL_HANDLE;
    texture->SizeInBytes = 0;
    texture->State.store(TextureState::Released, std::memory_order_release);
}

void VulkanRenderer::CleanUpFailedTextures()
{
    std::lock_guard lock(m_Mutex);

    for (auto it = m_Textures.begin(); it != m_Textures.end();)
    {
        const auto& texture = it->second;
        if (!texture || texture->State.load(std::memory_order_acquire) == TextureState::Failed)
            it = m_Textures.erase(it);
        else
            ++it;
    }
}

void VulkanRenderer::CleanUpUnusedTextures(uint64_t budget, uint64_t currentUsage)
{
    constexpr uint64_t KeepAliveFrames = 60 * 5;
    const uint64_t currentFrame = ImGui::GetFrameCount();

    std::vector<std::shared_ptr<VulkanTexture>> candidates;
    std::lock_guard lock(m_Mutex);

    for (const auto& [asset, texture] : m_Textures)
    {
        if (!texture || texture->State.load(std::memory_order_acquire) != TextureState::Ready ||
            currentFrame - texture->LastUsedFrame < KeepAliveFrames)
            continue;
        candidates.push_back(texture);
    }

    std::ranges::sort(candidates, std::greater{}, &VulkanTexture::SizeInBytes);

    for (const auto& texture : candidates)
    {
        const auto it = std::ranges::find_if(m_Textures, [&](const auto& pair) { return pair.second == texture; });
        if (it == m_Textures.end())
            continue;

        currentUsage = currentUsage >= texture->SizeInBytes ? currentUsage - texture->SizeInBytes : 0;
        m_ReleaseQueue.push(texture);
        m_Textures.erase(it);

        if (static_cast<double>(currentUsage) / static_cast<double>(budget) < CLEANUP_THRESHOLD_DESIRED)
            break;
    }
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("VulkanRenderer: no compatible memory type");
}

uint64_t VulkanRenderer::GetDeviceLocalMemoryBudget() const
{
    if (m_PhysicalDevice == VK_NULL_HANDLE)
        return 0;

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

    uint64_t budget = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        if ((memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
            budget += memoryProperties.memoryHeaps[i].size;
    }
    return budget;
}
