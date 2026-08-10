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
    void Shutdown();

    [[nodiscard]] RenderThread* GetRenderThread() const noexcept
    {
        return m_RenderThread;
    }

  private:
    RenderThread* m_RenderThread = nullptr;

    std::unique_ptr<DX12GpuUploader> m_GpuUploader;
    std::unordered_map<std::string, std::shared_ptr<DX12Texture>> m_Textures;

    // Inherited via IRenderer
    virtual [[nodiscard]] Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
    virtual void ReleaseTexture(const std::string& path) override;
    virtual void CleanUnusedTextures(const std::unordered_set<ImTextureID>& usedTextures) override;
    virtual [[nodiscard]] RendererType GetType() noexcept override;
    virtual void Reset() override;
    virtual void SetRenderThread(RenderThread* renderThread) override;
};
