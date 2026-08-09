// clang-format off
#include "pch.h"
// clang-format on

#include "imgui_drawdata_snapshot.h"

void ImGuiDrawDataSnapshot::Update(const ImDrawData* drawData)
{
    const int writeIndex = AcquireWriteBuffer();

    if (writeIndex == -1)
        return;

    Buffer& buffer = m_Buffers[writeIndex];

    buffer.Data.CopyFrom(drawData);

    buffer.State.store(BufferState::Ready, std::memory_order_release);
}

void ImGuiDrawDataSnapshot::Clear()
{
    const int writeIndex = AcquireWriteBuffer();

    if (writeIndex == -1)
        return;

    Buffer& buffer = m_Buffers[writeIndex];

    buffer.TextureUsage.clear();
    buffer.Data.Clear();
}

void ImGuiDrawDataSnapshot::AddImageUsage(ImTextureID textureID)
{
    const int writeIndex = AcquireWriteBuffer();

    if (writeIndex == -1)
        return;

    Buffer& buffer = m_Buffers[writeIndex];

    if (buffer.TextureUsage.find(textureID) == buffer.TextureUsage.end())
    {
        buffer.TextureUsage[textureID] = 1;
    }
    else
    {
        buffer.TextureUsage[textureID]++;
    }
}

std::unordered_set<ImTextureID> ImGuiDrawDataSnapshot::CollectUsedImages()
{
    std::unordered_set<ImTextureID> result;

    for (int i = 0; i < BufferCount; ++i)
    {
        Buffer& buffer = m_Buffers[i];

        for (const auto& [textureID, usage] : buffer.TextureUsage)
        {
            result.insert(textureID);
        }
    }

    return result;
}

ImDrawData* ImGuiDrawDataSnapshot::BeginRead()
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

    Buffer& buffer = m_Buffers[currentIndex];
    return buffer.Data.GetDrawData();
}

void ImGuiDrawDataSnapshot::EndRead()
{
    // const int index = m_ReadIndex.load(std::memory_order_acquire);
    //
    // if (index < 0 || index >= BufferCount)
    //     return;
    //
    // m_Buffers[index].State.store(BufferState::Free, std::memory_order_release);
}
