#pragma once
#include "pch.h"

#include "../IRenderer.h"
#include "DX12GpuUploader.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DX12Renderer : public IRenderer
{
  public:
    DX12Renderer();

    ~DX12Renderer();

    DX12Renderer(const DX12Renderer&) = delete;
    DX12Renderer& operator=(const DX12Renderer&) = delete;

    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, UINT capacity, UINT offset);

    [[nodiscard]] RenderThread* GetRenderThread() const noexcept
    {
        return m_RenderThread;
    }

  private:
    static constexpr double CLEANUP_THRESHOLD = 0.90;
    static constexpr double CLEANUP_THRESHOLD_DESIRED = 0.60;

    void Shutdown();
    ComPtr<ID3D12Device> m_Device;
    ComPtr<IDXGIAdapter3> m_Adapter;

    RenderThread* m_RenderThread = nullptr;

    std::unique_ptr<DX12GpuUploader> m_GpuUploader;
    std::unordered_map<std::shared_ptr<TextureAsset>, std::shared_ptr<DX12Texture>> m_Textures;

    // Inherited via IRenderer
    virtual [[nodiscard]] Texture UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset) override;
    virtual void CleanUpTextures() override;
    virtual void MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures) override;
    virtual [[nodiscard]] RendererType GetType() noexcept override;
    virtual void Reset() override;
    virtual void SetRenderThread(RenderThread* renderThread) override;

    void CleanUpFailedTextures();
    DXGI_QUERY_VIDEO_MEMORY_INFO GetMemoryUsage();
    void CleanUpUnusedTextures(DXGI_QUERY_VIDEO_MEMORY_INFO& memoryInfo);
};
