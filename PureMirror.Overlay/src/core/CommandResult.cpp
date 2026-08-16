#include "pch.h"

#include "CommandResult.h"

namespace PureMirror::Overlay
{
    CommandResult CommandResult::Success(std::string message)
    {
        return {.m_Status = CommandStatus::Executed, .m_Message = std::move(message)};
    }

    CommandResult CommandResult::Failure(std::string message)
    {
        return {.m_Status = CommandStatus::Failed, .m_Message = std::move(message)};
    }
}  // namespace PureMirror::Overlay
