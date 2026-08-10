#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <d3d12.h>

using Microsoft::WRL::ComPtr;

struct DX12Descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu{};

    uint32_t Index = UINT32_MAX;

    bool IsValid() const
    {
        return Index != UINT32_MAX;
    }
};

class DX12DescriptorAllocator
{
  public:
    DX12DescriptorAllocator(ID3D12Device* device, ID3D12DescriptorHeap* heap, uint32_t capacity);

    DX12DescriptorAllocator(const DX12DescriptorAllocator&) = delete;

    DX12DescriptorAllocator& operator=(const DX12DescriptorAllocator&) = delete;

    DX12Descriptor Allocate();

    void Free(const DX12Descriptor& descriptor);

    uint32_t GetCapacity() const
    {
        return m_Capacity;
    }

  private:
    static constexpr int INDEX_OFFSET = 128;
    ID3D12DescriptorHeap* m_Heap = nullptr;

    uint32_t m_Capacity = 0;
    uint32_t m_DescriptorSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart{};

    std::queue<uint32_t> m_FreeIndices;

    mutable std::mutex m_Mutex;
};
