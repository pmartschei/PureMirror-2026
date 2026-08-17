#pragma once

#include "SemanticVersion.h"

#include <string_view>

namespace PureMirror::Overlay
{
    class SemanticVersionRange
    {
      public:
        [[nodiscard]] static bool IsValid(std::string_view expression);
        [[nodiscard]] static bool Contains(std::string_view expression, const SemanticVersion& version);
    };
}  // namespace PureMirror::Overlay
