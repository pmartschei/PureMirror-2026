#include "pch.h"

#include "VulkanRenderer.h"

#include <hooks/backend/vulkan/hook_vulkan.h>

VulkanRenderer::VulkanRenderer() {}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(VkPhysicalDevice physicalDevice,
                                VkDevice device,
                                VkQueue queue,
                                uint32_t queueFamily,
                                VkDescriptorPool descriptorPool,
                                const VkAllocationCallbacks* allocator)
{
    if (!m_GpuUploader)
    {
        m_GpuUploader =
            std::make_unique<VulkanGpuUploader>(physicalDevice, device, queue, queueFamily, descriptorPool, allocator);
    }
}

void VulkanRenderer::ProcessPendingTextures()
{
    if (m_GpuUploader)
        m_GpuUploader->ProcessPendingTextures();
}

void VulkanRenderer::Shutdown()
{
    if (!m_GpuUploader)
        return;

    for (auto& [asset, texture] : m_Textures)
        m_GpuUploader->ReleaseTexture(texture);

    m_Textures.clear();
    m_GpuUploader.reset();
}

Texture VulkanRenderer::UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset)
{
    if (!asset || !m_GpuUploader)
        return {};

    if (const auto it = m_Textures.find(asset); it != m_Textures.end())
    {
        const auto& texture = it->second;
        Texture result{};
        result.Size = ImVec2(static_cast<float>(texture->Width), static_cast<float>(texture->Height));

        if (m_GpuUploader->IsReady(texture))
            result.TextureID = reinterpret_cast<ImTextureID>(texture->DescriptorSet);

        return result;
    }

    auto texture = m_GpuUploader->UploadTexture(asset);
    if (!texture)
        return {};

    m_Textures.emplace(asset, texture);

    Texture result{};
    result.Size = ImVec2(static_cast<float>(texture->Width), static_cast<float>(texture->Height));
    return result;
}

void VulkanRenderer::CleanUpTextures()
{
    if (!m_GpuUploader)
        return;

    CleanUpFailedTextures();

    const uint64_t budget = m_GpuUploader->GetDeviceLocalMemoryBudget();
    const uint64_t usage = m_GpuUploader->GetAllocatedBytes();

    if (budget == 0 || static_cast<double>(usage) / static_cast<double>(budget) < CLEANUP_THRESHOLD)
        return;

    CleanUpUnusedTextures(budget, usage);
}

void VulkanRenderer::MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures)
{
    if (!m_GpuUploader)
        return;

    const uint64_t currentFrame = ImGui::GetFrameCount();

    for (auto& [asset, texture] : m_Textures)
    {
        if (m_GpuUploader->IsReady(texture) &&
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

void VulkanRenderer::CleanUpFailedTextures()
{
    for (auto it = m_Textures.begin(); it != m_Textures.end();)
    {
        const auto& texture = it->second;
        if (!texture || texture->State.load(std::memory_order_acquire) == VulkanTextureState::Failed)
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

    for (const auto& [asset, texture] : m_Textures)
    {
        if (!m_GpuUploader->IsReady(texture) || currentFrame - texture->LastUsedFrame < KeepAliveFrames)
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
        m_GpuUploader->ReleaseTexture(texture);
        m_Textures.erase(it);

        if (static_cast<double>(currentUsage) / static_cast<double>(budget) < CLEANUP_THRESHOLD_DESIRED)
            break;
    }
}
