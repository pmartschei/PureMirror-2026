#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "imgui_drawdata_copy.h"

#include <imgui.h>

class ImGuiDrawDataSnapshot
{
  public:
    ImGuiDrawDataSnapshot() = default;

    ImGuiDrawDataSnapshot(const ImGuiDrawDataSnapshot&) = delete;
    ImGuiDrawDataSnapshot& operator=(const ImGuiDrawDataSnapshot&) = delete;

    void Update(const ImDrawData* drawData);
    void Clear();
    void AddImageUsage(ImTextureID textureID);

    std::unordered_set<ImTextureID> CollectUsedImages();

    ImDrawData* BeginRead();
    void EndRead();

  private:
    static constexpr int BufferCount = 2;

    enum class BufferState : uint8_t
    {
        Free,
        Writing,
        Ready,
        Reading
    };
    struct Buffer
    {
        ImGuiDrawDataCopy Data;
        std::unordered_map<ImTextureID, UINT> TextureUsage;
        std::atomic<BufferState> State{BufferState::Free};
    };
    int AcquireWriteBuffer()
    {
        // Check if we are currently writing some buffer
        for (int i = 0; i < BufferCount; ++i)
        {
            if (m_Buffers[i].State.load(std::memory_order_acquire) == BufferState::Writing)
            {
                return i;
            }
        }

        // Otherwise take a free one
        for (int i = 0; i < BufferCount; ++i)
        {
            BufferState expected = BufferState::Free;

            if (m_Buffers[i].State.compare_exchange_strong(
                    expected, BufferState::Writing, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return i;
            }
        }

        return -1;
    }

    std::array<Buffer, BufferCount> m_Buffers;

    std::atomic<int> m_ReadIndex{0};
};
