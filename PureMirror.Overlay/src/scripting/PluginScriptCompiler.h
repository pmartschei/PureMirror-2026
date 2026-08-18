#pragma once

#include "IScriptEngine.h"
#include "src/plugins/PluginManifest.h"
#include "src/plugins/PluginPackage.h"

#include <filesystem>
#include <vector>

namespace PureMirror::Overlay
{
    class PluginScriptCompiler
    {
      public:
        explicit PluginScriptCompiler(IScriptEngine& scriptEngine);

        [[nodiscard]] ScriptModuleLoadResult Compile(const PluginManifest& manifest,
                                                     const std::filesystem::path& packageRoot,
                                                     const std::vector<PluginPackage>& dependencies = {}) const;

      private:
        IScriptEngine& m_ScriptEngine;
    };
}  // namespace PureMirror::Overlay
