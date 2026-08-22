#pragma once

#include "ScriptCallbackArgument.h"
#include "ScriptCallbackTag.h"

#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    struct ScriptCallback
    {
        std::string_view FunctionDeclaration;
        ScriptCallbackTag Tags{ScriptCallbackTag::None};
        std::vector<ScriptCallbackArgument> Arguments;
    };
}  // namespace PureMirror::Overlay
