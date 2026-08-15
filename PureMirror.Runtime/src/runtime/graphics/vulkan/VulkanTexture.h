#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <imgui.h>
#include <vulkan/vulkan.h>

enum class VulkanTextureState : uint8_t
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

    std::atomic<VulkanTextureState> State{VulkanTextureState::Pending};
};
