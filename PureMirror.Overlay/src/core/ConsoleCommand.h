#pragma once

#include "CommandResult.h"

#include <functional>
#include <string>
#include <string_view>

namespace PureMirror::Overlay
{
    using CommandHandler = std::function<CommandResult(std::string_view arguments)>;

    struct ConsoleCommand
    {
        std::string m_Name;
        std::string m_Description;
        std::string m_Origin;
        CommandHandler m_Handler;
    };
}  // namespace PureMirror::Overlay
