// clang-format off
#include "pch.h"
// clang-format on

#include "VulkanRenderer.h"

#include <hooks/backend/vulkan/hook_vulkan.h>

VulkanRenderer::VulkanRenderer() {}

VulkanRenderer::~VulkanRenderer() {}

Texture VulkanRenderer::UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset)
{
    return Texture();
}

void VulkanRenderer::CleanUpTextures() {}

void VulkanRenderer::MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) {}

RendererType VulkanRenderer::GetType() noexcept
{
    return RendererType::Vulkan;
}

void VulkanRenderer::Reset()
{
    SetRenderThread(nullptr);
    VK::Unhook();
}

void VulkanRenderer::SetRenderThread(RenderThread* renderThread)
{
    m_RenderThread = renderThread;
}
