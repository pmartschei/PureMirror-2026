#pragma once

#include "IScriptHost.h"
#include "src/core/logger/Logger.h"

namespace PureMirror::Overlay
{
    class OverlayScriptHost final : public IScriptHost
    {
      public:
        explicit OverlayScriptHost(Logger& logger);

        void LogInfo(std::string_view pluginId, std::string_view message) override;
        [[nodiscard]] bool BeginWindow(std::string_view pluginId, std::string_view title) override;
        void EndWindow(std::string_view pluginId) override;
        void Text(std::string_view pluginId, std::string_view value) override;
        [[nodiscard]] bool Button(std::string_view pluginId, std::string_view label) override;

      private:
        Logger& m_Logger;
    };
}  // namespace PureMirror::Overlay
