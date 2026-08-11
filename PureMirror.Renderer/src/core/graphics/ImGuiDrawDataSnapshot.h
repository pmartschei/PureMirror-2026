#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "ImGuiDrawDataCopy.h"

#include <imgui.h>

struct ReadyBuffer
{
    int Index = -1;
    uint64_t Generation = 0;
};

class ImGuiDrawDataSnapshot
{
  public:
    ImGuiDrawDataSnapshot() = default;

    ImGuiDrawDataSnapshot(const ImGuiDrawDataSnapshot&) = delete;
    ImGuiDrawDataSnapshot& operator=(const ImGuiDrawDataSnapshot&) = delete;

    ImGuiDrawDataSnapshot(ImGuiDrawDataSnapshot&&) noexcept = default;
    ImGuiDrawDataSnapshot& operator=(ImGuiDrawDataSnapshot&&) noexcept = default;

    void BeginUpdate() noexcept;
    void Update(const ImDrawData* drawData) noexcept;

    [[nodiscard]] ImDrawData* BeginRead() noexcept;

    [[nodiscard]] std::unordered_set<ImTextureID> CollectUsedImages();

  private:
    // Thread ownership:
    // - m_WriteIndex / m_WriteGeneration: writer thread only
    // - m_ReadIndex: reader thread only
    // - Buffer::State: synchronization between writer and reader

    static constexpr UINT BufferCount = 3;
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
        std::atomic<UINT64> Generation{0};
        std::atomic<BufferState> State{BufferState::Free};
    };

    std::array<Buffer, BufferCount> m_Buffers;
    UINT m_ReadIndex = -1;
    UINT m_WriteIndex = -1;
    UINT64 m_WriteGeneration = 0;

    std::unordered_set<ImTextureID> m_UsedImages;

    [[nodiscard]] ReadyBuffer FindNewestReadyBuffer(int currentIndex, uint64_t currentGeneration) noexcept;
    void ReleaseOlderReadyBuffers(int keepIndex, uint64_t newestGeneration) noexcept;

    int AcquireWriteBuffer();
};
