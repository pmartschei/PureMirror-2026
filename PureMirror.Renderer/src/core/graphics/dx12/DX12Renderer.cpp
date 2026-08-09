// clang-format off
#include "pch.h"
// clang-format on

#include "DX12Renderer.h"

DX12Renderer::DX12Renderer() {}

DX12Renderer::~DX12Renderer() {}

void DX12Renderer::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap)
{
    m_GpuUploader = std::make_unique<DX12GpuUploader>(device, srvHeap, 2 << 12);
}

Texture DX12Renderer::UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset)
{
    if (!asset)
        return {};

    if (auto it = m_Textures.find(asset->Path); it != m_Textures.end())
    {
        const auto& dx12Texture = it->second;

        return {dx12Texture->ImGuiID,
                ImVec2(static_cast<float>(dx12Texture->Width), static_cast<float>(dx12Texture->Height))};
    }

    auto dx12Texture = m_GpuUploader->UploadTexture(asset);

    m_Textures.emplace(asset->Path, dx12Texture);

    Texture result{};

    result.Size = ImVec2(static_cast<float>(dx12Texture->Width), static_cast<float>(dx12Texture->Height));

    if (!dx12Texture || !m_GpuUploader->IsReady(dx12Texture))
        return result;

    result.TextureID = dx12Texture->ImGuiID;

    return result;
}
