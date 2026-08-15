#pragma once

#include "pch.h"

#include <d3d12.h>

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
    DX12DescriptorAllocator(ID3D12Device* device, ID3D12DescriptorHeap* heap, UINT capacity, UINT offset);

    DX12DescriptorAllocator(const DX12DescriptorAllocator&) = delete;

    DX12DescriptorAllocator& operator=(const DX12DescriptorAllocator&) = delete;

    DX12Descriptor Allocate();

    void Free(const DX12Descriptor& descriptor);

    [[nodiscard]] UINT GetCapacity() const noexcept
    {
        return m_Capacity;
    }

  private:
    static constexpr int INDEX_OFFSET = 128;
    ID3D12DescriptorHeap* m_Heap = nullptr;

    UINT m_Offset = 0;
    UINT m_Capacity = 0;
    UINT m_DescriptorSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuStart{};

    std::queue<UINT> m_FreeIndices;

    mutable std::mutex m_Mutex;
};
