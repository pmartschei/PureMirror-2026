#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include "../IRenderer.h"
#include "DX12GpuUploader.h"

class DX12Renderer : public IRenderer
{
  public:
    DX12Renderer();

    ~DX12Renderer();

    DX12Renderer(const DX12Renderer&) = delete;
    DX12Renderer& operator=(const DX12Renderer&) = delete;

    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap);

    void ReleaseTexture(const std::string& path) override;

  private:
    std::unique_ptr<DX12GpuUploader> m_GpuUploader;

    std::unordered_map<std::string, std::shared_ptr<DX12Texture>> m_Textures;

    virtual Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
};
