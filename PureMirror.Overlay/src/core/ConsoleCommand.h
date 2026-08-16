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
        std::string Name;
        std::string Description;
        std::string Origin;
        CommandHandler Handler;
    };
}  // namespace PureMirror::Overlay
