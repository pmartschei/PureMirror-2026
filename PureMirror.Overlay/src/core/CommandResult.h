#pragma once

#include "CommandStatus.h"

#include <string>

namespace PureMirror::Overlay
{
    struct CommandResult
    {
        CommandStatus Status{CommandStatus::Executed};
        std::string Message;

        static CommandResult Success(std::string message = {});
        static CommandResult Failure(std::string message);
    };
}  // namespace PureMirror::Overlay
