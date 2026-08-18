#pragma once

#include "IScriptHost.h"
#include "ScriptUiScopeStack.h"
#include "src/core/logger/Logger.h"

namespace PureMirror::Overlay
{
    class OverlayScriptHost final : public IScriptHost
    {
      public:
        explicit OverlayScriptHost(Logger& logger);

        void BeginScriptCall(std::string_view pluginId) override;
        void EndScriptCall(std::string_view pluginId) override;
        void LogInfo(std::string_view pluginId, std::string_view message) override;
        [[nodiscard]] bool BeginWindow(std::string_view pluginId, std::string_view title) override;
        void EndWindow(std::string_view pluginId) override;
        void Text(std::string_view pluginId, std::string_view value) override;
        [[nodiscard]] bool Button(std::string_view pluginId, std::string_view label) override;

      private:
        void RecoverOpenScopes();
        void LogRecoveredScope(const ScriptUiScope& scope);
        void LogUnexpectedClose(std::string_view pluginId, std::string_view closingCommand);

        Logger& m_Logger;
        ScriptUiScopeStack m_UiScopes;
    };
}  // namespace PureMirror::Overlay
