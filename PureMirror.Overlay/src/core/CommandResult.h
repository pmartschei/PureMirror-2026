#pragma once

#include "CommandStatus.h"

#include <string>

namespace PureMirror::Overlay
{
    struct CommandResult
    {
        CommandStatus m_Status{CommandStatus::Executed};
        std::string m_Message;

        static CommandResult Success(std::string message = {});
        static CommandResult Failure(std::string message);
    };
}  // namespace PureMirror::Overlay
