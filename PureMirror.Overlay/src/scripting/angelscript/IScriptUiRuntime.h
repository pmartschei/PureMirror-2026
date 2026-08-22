#pragma once

#include "bindings/math/ScriptImVec2.h"

#include <cstdint>
#include <string>

namespace PureMirror::Overlay
{
    class IScriptUiRuntime
    {
      public:
        virtual ~IScriptUiRuntime() = default;

        [[nodiscard]] virtual bool HostBegin(const std::string& name, bool* open, std::uint32_t flags) = 0;
        virtual void HostEnd() = 0;
        virtual void HostText(const std::string& text) = 0;
        [[nodiscard]] virtual bool HostButton(const std::string& label, const ScriptImVec2& size) = 0;
        [[nodiscard]] virtual bool HostBeginMenu(const std::string& label, bool enabled) = 0;
        virtual void HostEndMenu() = 0;
        [[nodiscard]] virtual bool HostMenuItem(const std::string& label,
                                                const std::string& shortcut,
                                                bool selected,
                                                bool enabled) = 0;
        virtual void HostSeparator() = 0;
    };
}  // namespace PureMirror::Overlay
