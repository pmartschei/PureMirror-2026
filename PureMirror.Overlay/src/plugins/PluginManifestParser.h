#pragma once

#include "PluginManifestParseResult.h"

#include <string_view>

namespace PureMirror::Overlay
{
    class PluginManifestParser
    {
      public:
        [[nodiscard]] PluginManifestParseResult Parse(std::string_view json) const;
    };
}  // namespace PureMirror::Overlay
