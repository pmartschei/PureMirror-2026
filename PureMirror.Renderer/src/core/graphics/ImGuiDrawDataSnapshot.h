#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "ImGuiDrawDataCopy.h"

#include <imgui.h>

class ImGuiDrawDataSnapshot
{
  public:
    ImGuiDrawDataSnapshot() = default;

    ImGuiDrawDataSnapshot(const ImGuiDrawDataSnapshot&) = delete;
    ImGuiDrawDataSnapshot& operator=(const ImGuiDrawDataSnapshot&) = delete;

    ImGuiDrawDataSnapshot(ImGuiDrawDataSnapshot&&) noexcept = default;
    ImGuiDrawDataSnapshot& operator=(ImGuiDrawDataSnapshot&&) noexcept = default;

    void Update(const ImDrawData* drawData) noexcept;
    void Clear() noexcept;
    void AddImageUsage(ImTextureID textureID) noexcept;

    [[nodiscard]] std::unordered_set<ImTextureID> CollectUsedImages() noexcept;

    [[nodiscard]] ImDrawData* BeginRead() noexcept;
    void EndRead() noexcept;

  private:
    // Keep this at 2, unless the bug where the UI can render an old snapshot is fixed
    // This could happen because the Buffers are not used from latest to newest, but used from 0 to BufferCount, so
    // everything larger than 2 can result in using new buffers first before using old buffers
    // can probably successfully reproduce this when the renderThread is higher fps than the game (unsure)
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

    std::atomic<UINT> m_ReadIndex{0};
};
