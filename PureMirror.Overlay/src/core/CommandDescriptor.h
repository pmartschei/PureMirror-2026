#pragma once

#include <string>

namespace PureMirror::Overlay
{
    struct CommandDescriptor
    {
        std::string m_Name;
        std::string m_Description;
        std::string m_Origin;
    };
}  // namespace PureMirror::Overlay
