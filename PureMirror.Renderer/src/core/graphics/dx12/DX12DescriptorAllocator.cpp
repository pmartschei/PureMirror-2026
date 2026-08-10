// clang-format off
#include "pch.h"
// clang-format on
#include "DX12DescriptorAllocator.h"

DX12DescriptorAllocator::DX12DescriptorAllocator(ID3D12Device* device, ID3D12DescriptorHeap* heap, uint32_t capacity)
    : m_Heap(heap), m_Capacity(capacity)
{
    if (!device)
        throw std::invalid_argument("DX12DescriptorAllocator: device is null");

    if (!heap)
        throw std::invalid_argument("DX12DescriptorAllocator: heap is null");

    if (capacity == 0)
        throw std::invalid_argument("DX12DescriptorAllocator: capacity is zero");

    m_DescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_CpuStart = heap->GetCPUDescriptorHandleForHeapStart();

    m_GpuStart = heap->GetGPUDescriptorHandleForHeapStart();

    for (uint32_t i = 0; i < capacity; ++i)
        m_FreeIndices.push(i);
}
DX12Descriptor DX12DescriptorAllocator::Allocate()
{
    std::lock_guard lock(m_Mutex);

    if (m_FreeIndices.empty())
        throw std::runtime_error("DX12DescriptorAllocator: descriptor heap exhausted");

    const uint32_t index = m_FreeIndices.front() + INDEX_OFFSET;

    m_FreeIndices.pop();

    DX12Descriptor descriptor{};

    descriptor.Index = index;

    descriptor.Cpu = m_CpuStart;

    descriptor.Cpu.ptr += static_cast<SIZE_T>(index) * m_DescriptorSize;

    descriptor.Gpu = m_GpuStart;

    descriptor.Gpu.ptr += static_cast<UINT64>(index) * m_DescriptorSize;

    return descriptor;
}
void DX12DescriptorAllocator::Free(const DX12Descriptor& descriptor)
{
    if (!descriptor.IsValid())
        return;

    if (descriptor.Index < 0 || descriptor.Index >= m_Capacity)
        return;

    std::lock_guard lock(m_Mutex);

    m_FreeIndices.push(descriptor.Index);
}
