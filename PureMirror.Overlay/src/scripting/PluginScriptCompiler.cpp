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
                                               const std::string& sectionName,
                                               ScriptModuleLoadResult& result)
        {
            std::error_code error;
            const auto sourcePath = std::filesystem::weakly_canonical(packageRoot / relativePath, error);
            if (error || !IsWithinRoot(sourcePath, packageRoot))
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Section = sectionName,
                                              .Message = "Script path leaves the plugin package."});
                return std::nullopt;
            }

            std::ifstream stream(sourcePath, std::ios::binary);
            if (!stream)
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Section = sectionName,
                                              .Message = "Script source could not be opened."});
                return std::nullopt;
            }
            return ScriptSource{.Name = sectionName,
                                .Code = {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}}};
        }
    }  // namespace

    PluginScriptCompiler::PluginScriptCompiler(IScriptEngine& scriptEngine) : m_ScriptEngine(scriptEngine) {}

    ScriptModuleLoadResult PluginScriptCompiler::Compile(const PluginManifest& manifest,
                                                         const std::filesystem::path& packageRoot,
                                                         const std::vector<PluginPackage>& dependencies) const
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
        const auto addSource =
            [&](const std::filesystem::path& root, const std::string& path, const std::string& sectionName)
        {
            if (!sourcePaths.insert(sectionName).second)
                return;
            auto source = ReadSource(root, path, sectionName, result);
            if (source)
                sources.push_back(std::move(*source));
        };
        addSource(canonicalRoot, manifest.Entry, manifest.Entry);
        for (const auto& dependency : dependencies)
        {
            const auto dependencyRoot = std::filesystem::weakly_canonical(dependency.Location, error);
            if (error || !std::filesystem::is_directory(dependencyRoot))
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Section = dependency.Manifest.Id,
                                              .Message = "Dependency package root does not exist."});
                error.clear();
                continue;
            }
            for (const auto& exportPath : dependency.Manifest.Exports)
                addSource(dependencyRoot, exportPath, dependency.Manifest.Id + '/' + exportPath);
        }
        if (!result.Diagnostics.empty())
            return result;

        return m_ScriptEngine.LoadModule(manifest.Id, sources);
    }
}  // namespace PureMirror::Overlay
