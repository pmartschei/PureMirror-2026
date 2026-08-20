#pragma once

#include "ScriptCallbackTag.h"

#include <string_view>

namespace PureMirror::Overlay
{
    struct ScriptCallback
    {
        std::string_view FunctionDeclaration;
        ScriptCallbackTag Tags{ScriptCallbackTag::None};
    };
}  // namespace PureMirror::Overlay
