// clang-format off
#include "pch.h"
// clang-format on

#include "ImGuiDrawDataCopy.h"

void ImGuiDrawDataCopy::CopyFrom(const ImDrawData* source) noexcept
{
    if (!source)
    {
        Clear();
        return;
    }

    m_UsedImages.clear();
    m_DrawLists.clear();
    m_DrawLists.reserve(source->CmdListsCount);

    for (int i = 0; i < source->CmdListsCount; ++i)
    {
        const ImDrawList* sourceList = source->CmdLists[i];

        auto drawList = std::make_unique<ImDrawList>(sourceList->_Data);

        drawList->CmdBuffer = sourceList->CmdBuffer;
        drawList->IdxBuffer = sourceList->IdxBuffer;
        drawList->VtxBuffer = sourceList->VtxBuffer;
        drawList->Flags = sourceList->Flags;

        for (const ImDrawCmd& cmd : drawList->CmdBuffer)
        {
            if (const ImTextureID id = cmd.GetTexID())
                m_UsedImages.insert(id);
        }

        m_DrawLists.push_back(std::move(drawList));
    }

    m_CmdLists.clear();
    m_CmdLists.reserve(m_DrawLists.size());

    for (auto& drawList : m_DrawLists)
        m_CmdLists.push_back(drawList.get());

    m_DrawData = *source;

    m_DrawData.CmdLists = m_CmdLists.data();
    m_DrawData.CmdListsCount = static_cast<int>(m_CmdLists.size());
}

void ImGuiDrawDataCopy::Clear() noexcept
{
    m_UsedImages.clear();
    m_DrawLists.clear();
    m_CmdLists.clear();
    m_DrawData = {};
}
