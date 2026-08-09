#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <d3d12.h>
#include <imgui.h>

using Microsoft::WRL::ComPtr;

struct DX12Texture
{
    ComPtr<ID3D12Resource> Resource;
    ComPtr<ID3D12Resource> UploadBuffer;

    DX12Descriptor Descriptor;

    ImTextureID ImGuiID{};

    uint64_t FenceValue = 0;

    uint32_t Width = 0;
    uint32_t Height = 0;

    std::atomic<bool> Ready = false;
};
