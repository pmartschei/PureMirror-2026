#pragma once

#include <string_view>

namespace PureMirror::Overlay
{
    class IScriptHost
    {
      public:
        virtual ~IScriptHost() = default;

        virtual void LogInfo(std::string_view pluginId, std::string_view message) = 0;
        [[nodiscard]] virtual bool BeginWindow(std::string_view pluginId, std::string_view title) = 0;
        virtual void EndWindow(std::string_view pluginId) = 0;
        virtual void Text(std::string_view pluginId, std::string_view value) = 0;
        [[nodiscard]] virtual bool Button(std::string_view pluginId, std::string_view label) = 0;
    };
}  // namespace PureMirror::Overlay
