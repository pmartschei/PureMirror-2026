#pragma once

#include <functional>
#include <string>

namespace PureMirror::Overlay
{
    struct ScriptUiScope
    {
        std::string OwnerId;
        std::string OpeningCommand;
        std::string ClosingCommand;
        std::function<void()> Close;
    };
}  // namespace PureMirror::Overlay
