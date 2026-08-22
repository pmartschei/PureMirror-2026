#pragma once

#include <cstdint>
#include <string_view>

namespace PureMirror::Overlay
{
    class IScriptHost
    {
      public:
        virtual ~IScriptHost() = default;

        virtual void BeginScriptCall(std::string_view pluginId) = 0;
        virtual void EndScriptCall(std::string_view pluginId) = 0;
        virtual void LogInfo(std::string_view pluginId, std::string_view message) = 0;
        [[nodiscard]] virtual bool BeginWindow(std::string_view pluginId,
                                               std::string_view title,
                                               bool* open,
                                               std::uint32_t flags) = 0;
        virtual void EndWindow(std::string_view pluginId) = 0;
        virtual void Text(std::string_view pluginId, std::string_view value) = 0;
        [[nodiscard]] virtual bool Button(std::string_view pluginId,
                                          std::string_view label,
                                          float width,
                                          float height) = 0;
        [[nodiscard]] virtual bool BeginMenu(std::string_view pluginId, std::string_view label, bool enabled) = 0;
        virtual void EndMenu(std::string_view pluginId) = 0;
        [[nodiscard]] virtual bool MenuItem(std::string_view pluginId,
                                            std::string_view label,
                                            std::string_view shortcut,
                                            bool selected,
                                            bool enabled) = 0;
        virtual void MenuSeparator(std::string_view pluginId) = 0;
    };
}  // namespace PureMirror::Overlay
