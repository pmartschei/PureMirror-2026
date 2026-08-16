#include "pch.h"

#include "CommandResult.h"

namespace PureMirror::Overlay
{
    CommandResult CommandResult::Success(std::string message)
    {
        return {.Status = CommandStatus::Executed, .Message = std::move(message)};
    }

    CommandResult CommandResult::Failure(std::string message)
    {
        return {.Status = CommandStatus::Failed, .Message = std::move(message)};
    }
}  // namespace PureMirror::Overlay
