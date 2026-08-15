#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <imgui.h>

class ImGuiDrawDataCopy
{
  public:
    ImGuiDrawDataCopy() = default;

    ImGuiDrawDataCopy(const ImGuiDrawDataCopy&) = delete;
    ImGuiDrawDataCopy& operator=(const ImGuiDrawDataCopy&) = delete;

    ImGuiDrawDataCopy(ImGuiDrawDataCopy&&) noexcept = default;
    ImGuiDrawDataCopy& operator=(ImGuiDrawDataCopy&&) noexcept = default;

    void CopyFrom(const ImDrawData* source) noexcept;
    void Clear() noexcept;

    [[nodiscard]] ImDrawData& GetDrawData() noexcept
    {
        return m_DrawData;
    }

    [[nodiscard]] const ImDrawData& GetDrawData() const noexcept
    {
        return m_DrawData;
    }

    [[nodiscard]] bool Empty() const noexcept
    {
        return m_DrawLists.empty();
    }

    [[nodiscard]] const std::unordered_set<ImTextureID>& GetUsedImages() const noexcept
    {
        return m_UsedImages;
    }

  private:
    ImDrawData m_DrawData{};

    std::vector<std::unique_ptr<ImDrawList>> m_DrawLists;
    std::vector<ImDrawList*> m_CmdLists;
    std::unordered_set<ImTextureID> m_UsedImages;
};
