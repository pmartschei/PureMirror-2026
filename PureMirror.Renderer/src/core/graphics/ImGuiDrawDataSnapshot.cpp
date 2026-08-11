// clang-format off
#include "pch.h"
// clang-format on

#include "ImGuiDrawDataSnapshot.h"

void ImGuiDrawDataSnapshot::BeginUpdate() noexcept
{
    if (m_WriteIndex != -1)
    {
        assert(false && "BeginUpdate called without calling Update");
        return;
    }

    m_WriteIndex = AcquireWriteBuffer();

    if (m_WriteIndex != -1)
        m_Buffers[m_WriteIndex].Data.Clear();
}

void ImGuiDrawDataSnapshot::Update(const ImDrawData* drawData) noexcept
{
    if (m_WriteIndex == -1)
        return;

    Buffer& buffer = m_Buffers[m_WriteIndex];

    buffer.Data.CopyFrom(drawData);

    const UINT64 generation = ++m_WriteGeneration;
    buffer.Generation.store(generation, std::memory_order_relaxed);
    buffer.State.store(BufferState::Ready, std::memory_order_release);

    m_WriteIndex = -1;
}

std::unordered_set<ImTextureID> ImGuiDrawDataSnapshot::CollectUsedImages()
{
    m_UsedImages.clear();

    for (int i = 0; i < BufferCount; ++i)
    {
        Buffer& buffer = m_Buffers[i];

        const BufferState state = buffer.State.load(std::memory_order_acquire);

        if (state != BufferState::Ready && state != BufferState::Reading)
        {
            continue;
        }

        const auto& usedImages = buffer.Data.GetUsedImages();

        m_UsedImages.insert(usedImages.cbegin(), usedImages.cend());
    }

    return m_UsedImages;
}

ImDrawData* ImGuiDrawDataSnapshot::BeginRead() noexcept
{
    const int currentIndex = m_ReadIndex;

    uint64_t currentGeneration = 0;

    if (currentIndex >= 0 && currentIndex < BufferCount)
    {
        currentGeneration = m_Buffers[currentIndex].Generation.load(std::memory_order_relaxed);
    }

    ReadyBuffer readyBuffer = FindNewestReadyBuffer(currentIndex, currentGeneration);

    int newestIndex = readyBuffer.Index;
    uint64_t newestGeneration = readyBuffer.Generation;

    if (newestIndex != -1)
    {
        BufferState expected = BufferState::Ready;

        if (m_Buffers[newestIndex].State.compare_exchange_strong(
                expected, BufferState::Reading, std::memory_order_acquire, std::memory_order_relaxed))
        {
            m_ReadIndex = newestIndex;

            if (currentIndex >= 0 && currentIndex < BufferCount)
            {
                m_Buffers[currentIndex].State.store(BufferState::Free, std::memory_order_release);
            }

            ReleaseOlderReadyBuffers(newestIndex, newestGeneration);

            return &m_Buffers[newestIndex].Data.GetDrawData();
        }
    }

    // return current data if we do not have new data
    if (currentIndex >= 0 && currentIndex < BufferCount)
    {
        return &m_Buffers[currentIndex].Data.GetDrawData();
    }

    // no data
    return nullptr;
}

ReadyBuffer ImGuiDrawDataSnapshot::FindNewestReadyBuffer(int currentIndex, uint64_t currentGeneration) noexcept
{
    int newestIndex = -1;
    UINT64 newestGeneration = currentGeneration;

    for (int i = 0; i < BufferCount; ++i)
    {
        if (i == currentIndex)
            continue;

        Buffer& buffer = m_Buffers[i];

        if (buffer.State.load(std::memory_order_acquire) != BufferState::Ready)
            continue;

        const UINT64 generation = buffer.Generation.load(std::memory_order_relaxed);

        if (generation > newestGeneration)
        {
            newestGeneration = buffer.Generation;
            newestIndex = i;
        }
    }

    return ReadyBuffer{.Index = newestIndex, .Generation = newestGeneration};
}

void ImGuiDrawDataSnapshot::ReleaseOlderReadyBuffers(int keepIndex, uint64_t newestGeneration) noexcept
{
    for (int i = 0; i < BufferCount; ++i)
    {
        if (i == keepIndex)
            continue;

        Buffer& buffer = m_Buffers[i];

        if (buffer.State.load(std::memory_order_acquire) != BufferState::Ready)
            continue;

        const UINT64 generation = buffer.Generation.load(std::memory_order_relaxed);

        if (generation >= newestGeneration)
            continue;

        BufferState expected = BufferState::Ready;

        buffer.State.compare_exchange_strong(
            expected, BufferState::Free, std::memory_order_release, std::memory_order_relaxed);
    }
}

int ImGuiDrawDataSnapshot::AcquireWriteBuffer()
{
    // Prefer free buffers.
    for (int i = 0; i < BufferCount; ++i)
    {
        BufferState expected = BufferState::Free;

        if (m_Buffers[i].State.compare_exchange_strong(
                expected, BufferState::Writing, std::memory_order_acquire, std::memory_order_relaxed))
        {
            return i;
        }
    }

    // No free buffer. Reclaim the oldest Ready buffer.
    int oldestIndex = -1;
    UINT64 oldestGeneration = UINT64_MAX;

    for (int i = 0; i < BufferCount; ++i)
    {
        Buffer& buffer = m_Buffers[i];

        if (buffer.State.load(std::memory_order_acquire) != BufferState::Ready)
            continue;

        const UINT64 generation = buffer.Generation.load(std::memory_order_relaxed);

        if (generation < oldestGeneration)
        {
            oldestGeneration = buffer.Generation;
            oldestIndex = i;
        }
    }

    if (oldestIndex != -1)
    {
        BufferState expected = BufferState::Ready;

        if (m_Buffers[oldestIndex].State.compare_exchange_strong(
                expected, BufferState::Writing, std::memory_order_acquire, std::memory_order_relaxed))
        {
            return oldestIndex;
        }
    }

    return -1;
}
