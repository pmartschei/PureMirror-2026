// clang-format off
#include "pch.h"
// clang-format on

#include "ImGuiDrawDataSnapshot.h"

void ImGuiDrawDataSnapshot::Update(const ImDrawData* drawData) noexcept
{
    const int writeIndex = AcquireWriteBuffer();

    if (writeIndex == -1)
        return;

    Buffer& buffer = m_Buffers[writeIndex];

    buffer.Data.CopyFrom(drawData);

    buffer.State.store(BufferState::Ready, std::memory_order_release);
}

void ImGuiDrawDataSnapshot::Clear() noexcept
{
    const int writeIndex = AcquireWriteBuffer();

    if (writeIndex == -1)
        return;

    Buffer& buffer = m_Buffers[writeIndex];

    buffer.Data.Clear();
}

std::unordered_set<ImTextureID> ImGuiDrawDataSnapshot::CollectUsedImages()
{
    m_UsedImages.clear();

    for (int i = 0; i < BufferCount; ++i)
    {
        Buffer& buffer = m_Buffers[i];

        const auto& usedImages = buffer.Data.GetUsedImages();

        m_UsedImages.insert(usedImages.cbegin(), usedImages.cend());
    }

    return m_UsedImages;
}

ImDrawData* ImGuiDrawDataSnapshot::BeginRead() noexcept
{
    int currentIndex = m_ReadIndex.load(std::memory_order_acquire);

    if (currentIndex < 0 || currentIndex > BufferCount)
        return nullptr;

    for (int i = 0; i < BufferCount; ++i)
    {
        if (i == currentIndex)
            continue;

        BufferState expected = BufferState::Ready;

        if (m_Buffers[i].State.compare_exchange_strong(
                expected, BufferState::Reading, std::memory_order_acquire, std::memory_order_relaxed))
        {
            m_ReadIndex.store(i, std::memory_order_release);

            m_Buffers[currentIndex].State.store(BufferState::Free, std::memory_order_release);

            currentIndex = i;
            break;
        }
    }

    // This theoretically could break, with the current state handling

    assert(currentIndex >= 0 && currentIndex < static_cast<UINT>(m_Buffers.size()));

    Buffer& buffer = m_Buffers[currentIndex];
    return &buffer.Data.GetDrawData();
}

void ImGuiDrawDataSnapshot::EndRead() noexcept
{
    // const int index = m_ReadIndex.load(std::memory_order_acquire);
    //
    // if (index < 0 || index >= BufferCount)
    //     return;
    //
    // m_Buffers[index].State.store(BufferState::Free, std::memory_order_release);
}
