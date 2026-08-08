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

    void CopyFrom(const ImDrawData* source);
    void Clear();

    ImDrawData* GetDrawData()
    {
        return &m_DrawData;
    }

    const ImDrawData* GetDrawData() const
    {
        return &m_DrawData;
    }

    bool Empty() const
    {
        return m_DrawLists.empty();
    }

  private:
    ImDrawData m_DrawData{};

    std::vector<std::unique_ptr<ImDrawList>> m_DrawLists;
    std::vector<ImDrawList*> m_CmdLists;
};
