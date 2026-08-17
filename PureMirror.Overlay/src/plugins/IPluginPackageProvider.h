#pragma once

#include "PluginPackage.h"

#include <vector>

namespace PureMirror::Overlay
{
    class IPluginPackageProvider
    {
      public:
        virtual ~IPluginPackageProvider() = default;

        [[nodiscard]] virtual std::vector<PluginPackage> Discover() = 0;
    };
}  // namespace PureMirror::Overlay
