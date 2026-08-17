#include "pch.h"

#include "PluginManager.h"

#include "PluginDependencyResolver.h"
#include "PluginManifestParser.h"
#include "PluginVersionSolver.h"
#include "src/scripting/PluginScriptInstance.h"

#include <fstream>
#include <iterator>

namespace PureMirror::Overlay
{
    namespace
    {
        const LogOrigin PluginManagerOrigin{
            .Type = LogOriginType::Host, .Identifier = "puremirror.plugin-manager", .DisplayName = "Plugin Manager"};

        std::string DependencyIssueMessage(const PluginDependencyIssue& issue)
        {
            switch (issue.Type)
            {
            case PluginDependencyIssueType::DuplicatePlugin:
                return "Plugin '" + issue.PluginId + "' is available more than once.";
            case PluginDependencyIssueType::MissingDependency:
                return "Plugin '" + issue.PluginId + "' requires missing plugin '" + issue.DependencyId + "'.";
            case PluginDependencyIssueType::IncompatibleVersion:
                return "Plugin '" + issue.PluginId + "' requires '" + issue.DependencyId + "' " +
                       issue.RequiredVersion + ", but version " + issue.InstalledVersion + " is available.";
            case PluginDependencyIssueType::SelfDependency:
                return "Plugin '" + issue.PluginId + "' cannot depend on itself.";
            }
            return "Unknown plugin dependency error.";
        }

        std::string DiagnosticMessage(const ScriptDiagnostic& diagnostic)
        {
            std::string message;
            if (!diagnostic.Section.empty())
            {
                message = diagnostic.Section;
                if (diagnostic.Row != 0)
                    message += ':' + std::to_string(diagnostic.Row);
                message += ": ";
            }
            return message + diagnostic.Message;
        }
    }  // namespace

    PluginManager::PluginManager(IScriptEngine& scriptEngine, Logger& logger)
        : m_ScriptEngine(scriptEngine), m_Logger(logger)
    {
    }

    PluginManager::~PluginManager()
    {
        UnloadAll();
    }

    std::size_t PluginManager::LoadStartupPlugins(const std::filesystem::path& pluginsRoot)
    {
        UnloadAll();

        std::error_code error;
        if (!std::filesystem::is_directory(pluginsRoot, error))
        {
            m_Logger.Warning(PluginManagerOrigin,
                             "Plugin directory does not exist: " + pluginsRoot.string(),
                             "plugins.directory.missing");
            return 0;
        }

        std::vector<std::filesystem::path> packageRoots;
        for (std::filesystem::directory_iterator iterator(pluginsRoot, error), end; !error && iterator != end;
             iterator.increment(error))
        {
            if (iterator->is_directory(error) && std::filesystem::is_regular_file(iterator->path() / "plugin.json"))
                packageRoots.push_back(iterator->path());
        }
        if (error)
        {
            m_Logger.Error(PluginManagerOrigin,
                           "Could not scan plugin directory '" + pluginsRoot.string() + "': " + error.message(),
                           "plugins.directory.scan");
            return 0;
        }
        std::ranges::sort(packageRoots);

        PluginManifestParser parser;
        std::vector<PluginPackage> availablePackages;
        for (const auto& packageRoot : packageRoots)
        {
            const auto manifestPath = packageRoot / "plugin.json";
            std::ifstream stream(manifestPath, std::ios::binary);
            if (!stream)
            {
                m_Logger.Error(PluginManagerOrigin,
                               "Could not read plugin manifest: " + manifestPath.string(),
                               "plugins.manifest.read");
                continue;
            }

            const std::string json{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
            auto parseResult = parser.Parse(json);
            if (!parseResult.IsSuccessful())
            {
                for (const auto& manifestError : parseResult.Errors)
                    m_Logger.Error(PluginManagerOrigin,
                                   manifestPath.string() + " [" + manifestError.Field + "]: " + manifestError.Message,
                                   "plugins.manifest.invalid");
                continue;
            }
            availablePackages.push_back({.Manifest = std::move(parseResult.Manifest),
                                         .Origin = PluginPackageOrigin::Local,
                                         .Location = packageRoot.string()});
        }

        std::vector<std::string> rootPluginIds;
        rootPluginIds.reserve(availablePackages.size());
        for (const auto& package : availablePackages)
            rootPluginIds.push_back(package.Manifest.Id);
        std::ranges::sort(rootPluginIds);
        const auto uniqueRoot = std::ranges::unique(rootPluginIds);
        rootPluginIds.erase(uniqueRoot.begin(), uniqueRoot.end());

        const auto selection = PluginVersionSolver().Resolve(availablePackages, rootPluginIds);
        if (!selection.IsSuccessful())
        {
            for (const auto& selectionError : selection.Errors)
                m_Logger.Error(PluginManagerOrigin, selectionError.Message, "plugins.versions.invalid");
            return 0;
        }

        std::vector<PluginManifest> manifests;
        manifests.reserve(selection.Packages.size());
        for (const auto& package : selection.Packages)
            manifests.push_back(package.Manifest);

        const auto resolution = PluginDependencyResolver().Resolve(manifests);
        if (!resolution.IsSuccessful())
        {
            for (const auto& issue : resolution.Issues)
                m_Logger.Error(PluginManagerOrigin, DependencyIssueMessage(issue), "plugins.dependencies.invalid");
            return 0;
        }

        for (const auto& loadGroup : resolution.LoadGroups)
        {
            for (const auto& pluginId : loadGroup)
            {
                const auto selected = std::ranges::find(
                    selection.Packages, pluginId, [](const PluginPackage& package) { return package.Manifest.Id; });
                if (selected == selection.Packages.end())
                    continue;

                auto instance = std::make_unique<PluginScriptInstance>(
                    m_ScriptEngine, selected->Manifest, std::filesystem::path(selected->Location));
                const auto loadResult = instance->Load();
                if (!loadResult.IsSuccessful())
                {
                    for (const auto& diagnostic : loadResult.Diagnostics)
                        m_Logger.Error(PluginManagerOrigin,
                                       "Plugin '" + pluginId + "': " + DiagnosticMessage(diagnostic),
                                       "plugins.script.load");
                    UnloadAll();
                    return 0;
                }

                m_Logger.Info(PluginManagerOrigin,
                              "Loaded plugin '" + selected->Manifest.Name + "' " + selected->Manifest.Version + ".",
                              "plugins.loaded." + pluginId);
                m_LoadedPlugins.push_back(std::move(instance));
            }
        }
        return m_LoadedPlugins.size();
    }

    void PluginManager::Render()
    {
        for (auto plugin = m_LoadedPlugins.begin(); plugin != m_LoadedPlugins.end();)
        {
            const auto result = (*plugin)->Render();
            if (result.IsSuccessful())
            {
                ++plugin;
                continue;
            }

            for (const auto& diagnostic : result.Diagnostics)
                m_Logger.Error(PluginManagerOrigin,
                               "Plugin '" + result.ModuleId + "': " + DiagnosticMessage(diagnostic),
                               "plugins.script.render." + result.ModuleId);
            static_cast<void>((*plugin)->Unload());
            plugin = m_LoadedPlugins.erase(plugin);
        }
    }

    void PluginManager::UnloadAll()
    {
        for (auto plugin = m_LoadedPlugins.rbegin(); plugin != m_LoadedPlugins.rend(); ++plugin)
        {
            const auto result = (*plugin)->Unload();
            for (const auto& diagnostic : result.Diagnostics)
                m_Logger.Error(PluginManagerOrigin,
                               "Plugin '" + result.ModuleId + "': " + DiagnosticMessage(diagnostic),
                               "plugins.script.unload." + result.ModuleId);
        }
        m_LoadedPlugins.clear();
    }

    std::size_t PluginManager::LoadedPluginCount() const noexcept
    {
        return m_LoadedPlugins.size();
    }
}  // namespace PureMirror::Overlay
