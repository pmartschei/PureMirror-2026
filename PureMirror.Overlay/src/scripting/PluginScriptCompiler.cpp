#include "pch.h"

#include "PluginScriptCompiler.h"

#include <fstream>
#include <unordered_set>

namespace PureMirror::Overlay
{
    namespace
    {
        bool IsWithinRoot(const std::filesystem::path& path, const std::filesystem::path& root)
        {
            const auto relative = path.lexically_relative(root);
            return !relative.empty() && *relative.begin() != "..";
        }

        std::optional<ScriptSource> ReadSource(const std::filesystem::path& packageRoot,
                                               const std::string& relativePath,
                                               ScriptModuleLoadResult& result)
        {
            std::error_code error;
            const auto sourcePath = std::filesystem::weakly_canonical(packageRoot / relativePath, error);
            if (error || !IsWithinRoot(sourcePath, packageRoot))
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Section = relativePath,
                                              .Message = "Script path leaves the plugin package."});
                return std::nullopt;
            }

            std::ifstream stream(sourcePath, std::ios::binary);
            if (!stream)
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Section = relativePath,
                                              .Message = "Script source could not be opened."});
                return std::nullopt;
            }
            return ScriptSource{.Name = relativePath,
                                .Code = {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}}};
        }
    }  // namespace

    PluginScriptCompiler::PluginScriptCompiler(IScriptEngine& scriptEngine) : m_ScriptEngine(scriptEngine) {}

    ScriptModuleLoadResult PluginScriptCompiler::Compile(const PluginManifest& manifest,
                                                         const std::filesystem::path& packageRoot) const
    {
        ScriptModuleLoadResult result{.ModuleId = manifest.Id};
        std::error_code error;
        const auto canonicalRoot = std::filesystem::weakly_canonical(packageRoot, error);
        if (error || !std::filesystem::is_directory(canonicalRoot))
        {
            result.Diagnostics.push_back(
                {.Severity = ScriptDiagnosticSeverity::Error, .Message = "Plugin package root does not exist."});
            return result;
        }

        std::vector<ScriptSource> sources;
        std::unordered_set<std::string> sourcePaths;
        const auto addSource = [&](const std::string& path)
        {
            if (!sourcePaths.insert(path).second)
                return;
            auto source = ReadSource(canonicalRoot, path, result);
            if (source)
                sources.push_back(std::move(*source));
        };
        addSource(manifest.Entry);
        for (const auto& exportPath : manifest.Exports)
            addSource(exportPath);
        if (!result.Diagnostics.empty())
            return result;

        return m_ScriptEngine.LoadModule(manifest.Id, sources);
    }
}  // namespace PureMirror::Overlay
