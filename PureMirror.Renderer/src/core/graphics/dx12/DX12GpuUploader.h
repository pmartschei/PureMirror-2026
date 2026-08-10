#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "DX12DescriptorAllocator.h"
#include "DX12Texture.h"
#include "core/graphics/TextureAsset.h"

using Microsoft::WRL::ComPtr;

class DX12GpuUploader
{
  public:
    DX12GpuUploader(ID3D12Device* device,
                    ID3D12DescriptorHeap* srvHeap,
                    UINT descriptorCapacity,
                    UINT descriptorOffset);

    ~DX12GpuUploader();

    DX12GpuUploader(const DX12GpuUploader&) = delete;
    DX12GpuUploader& operator=(const DX12GpuUploader&) = delete;

    [[nodiscard]] std::shared_ptr<DX12Texture> UploadTexture(std::shared_ptr<TextureAsset> asset);

    void ReleaseTexture(std::shared_ptr<DX12Texture> texture);

    [[nodiscard]] bool IsReady(const std::shared_ptr<DX12Texture>& texture) const;

  private:
    enum class RequestType
    {
        Upload,
        Release,
    };

    struct UploadRequest
    {
        RequestType Type = RequestType::Upload;
        std::shared_ptr<TextureAsset> Asset;
        std::shared_ptr<DX12Texture> Texture;
    };

    void ThreadMain(std::stop_token stopToken);

    void UploadTextureInternal(const UploadRequest& request);

    void ReleaseTextureInternal(const std::shared_ptr<DX12Texture>& texture);

    void WaitForFence(uint64_t value);

  private:
    ComPtr<ID3D12Device> m_Device;

    DX12DescriptorAllocator m_DescriptorAllocator;
    // Dedicated COPY queue.
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    ComPtr<ID3D12Fence> m_Fence;

    HANDLE m_FenceEvent = nullptr;

    UINT m_DescriptorSize = 0;

    std::atomic<uint64_t> m_NextFenceValue = 1;

    std::mutex m_Mutex;
    std::condition_variable_any m_Condition;

    std::queue<UploadRequest> m_Queue;

    std::jthread m_Thread;
};
