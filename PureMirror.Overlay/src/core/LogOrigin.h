#pragma once

#include "LogOriginType.h"

#include <string>

namespace PureMirror::Overlay
{
    struct LogOrigin
    {
        LogOriginType Type{LogOriginType::Host};
        std::string Identifier{"puremirror"};
        std::string DisplayName{"PureMirror"};

        bool operator==(const LogOrigin&) const = default;
    };
}  // namespace PureMirror::Overlay
