#include "pch.h"

#include "PluginManager.h"

#include "PluginDependencyResolver.h"
#include "PluginManifestParser.h"
#include "PluginPackagePlanner.h"
#include "PluginReloadPlanner.h"
#include "PluginVersionSolver.h"
#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"
#include "src/scripting/PluginScriptInstance.h"

#include <fstream>
#include <iterator>
#include <map>
#include <tuple>
#include <unordered_set>

namespace PureMirror::Overlay
{
    namespace
    {
        const LogOrigin PluginManagerOrigin{
            .Type = LogOriginType::Host, .Identifier = "puremirror.plugin-manager", .DisplayName = "Plugin Manager"};

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

        void LogPlanErrors(Logger& logger, const std::vector<PluginVersionSelectionError>& errors)
        {
            for (const auto& error : errors)
                logger.Error(PluginManagerOrigin, error.Message, "plugins.selection." + error.PluginId);
        }

        std::vector<std::string> ExplicitPluginIds(const std::vector<PluginInstallation>& installations)
        {
            std::vector<std::string> result;
            for (const auto& installation : installations)
                if (installation.IsExplicit)
                    result.push_back(installation.Package.Manifest.Id);
            return result;
        }

        bool ContainsPlugin(const std::vector<PluginPackage>& packages, const std::string_view pluginId)
        {
            return std::ranges::find(
                       packages, pluginId, [](const PluginPackage& package) { return package.Manifest.Id; }) !=
                   packages.end();
        }

        std::vector<PluginPackage> DependencyPackages(const PluginPackage& package,
                                                      const std::vector<PluginInstallation>& installations)
        {
            std::vector<PluginPackage> result;
            std::unordered_set<std::string> addedIds;
            const auto addDependency = [&](const PluginDependency& dependency)
            {
                const auto installation =
                    std::ranges::find(installations,
                                      dependency.Id,
                                      [](const PluginInstallation& value) { return value.Package.Manifest.Id; });
                if (installation == installations.end())
                    return;
                const auto version = SemanticVersion::Parse(installation->Package.Manifest.Version);
                if (!version || !SemanticVersionRange::Contains(dependency.VersionRange, *version))
                    return;
                if (addedIds.insert(dependency.Id).second)
                    result.push_back(installation->Package);
            };
            for (const auto& dependency : package.Manifest.Dependencies)
                addDependency(dependency);
            for (const auto& dependency : package.Manifest.OptionalDependencies)
                addDependency(dependency);
            return result;
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

    std::size_t PluginManager::ScanPlugins(const std::filesystem::path& pluginsRoot)
    {
        UnloadAll();
        m_AvailablePackages.clear();

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
            m_AvailablePackages.push_back({.Manifest = std::move(parseResult.Manifest),
                                           .Origin = PluginPackageOrigin::Local,
                                           .Location = packageRoot.string()});
        }

        return AvailablePlugins().size();
    }

    std::vector<PluginInfo> PluginManager::AvailablePlugins() const
    {
        std::map<std::string, const PluginPackage*> packagesById;
        for (const auto& package : m_AvailablePackages)
        {
            const auto current = packagesById.find(package.Manifest.Id);
            if (current == packagesById.end())
            {
                packagesById.emplace(package.Manifest.Id, &package);
                continue;
            }

            const auto currentVersion = SemanticVersion::Parse(current->second->Manifest.Version);
            const auto candidateVersion = SemanticVersion::Parse(package.Manifest.Version);
            if (currentVersion && candidateVersion && *candidateVersion > *currentVersion)
                current->second = &package;
        }

        std::vector<PluginInfo> result;
        for (const auto& [id, package] : packagesById)
        {
            if (IsPluginLoaded(id))
                continue;
            result.push_back(
                {.Id = id, .Name = package->Manifest.Name, .Version = package->Manifest.Version, .IsExplicit = false});
        }
        std::ranges::sort(result,
                          [](const PluginInfo& left, const PluginInfo& right)
                          { return std::tie(left.Name, left.Id) < std::tie(right.Name, right.Id); });
        return result;
    }

    std::vector<PluginInfo> PluginManager::LoadedPlugins() const
    {
        std::vector<PluginInfo> result;
        result.reserve(m_Installations.size());
        for (const auto& installation : m_Installations)
        {
            if (!IsPluginLoaded(installation.Package.Manifest.Id))
                continue;
            result.push_back({.Id = installation.Package.Manifest.Id,
                              .Name = installation.Package.Manifest.Name,
                              .Version = installation.Package.Manifest.Version,
                              .IsExplicit = installation.IsExplicit});
        }
        std::ranges::sort(result,
                          [](const PluginInfo& left, const PluginInfo& right)
                          { return std::tie(left.Name, left.Id) < std::tie(right.Name, right.Id); });
        return result;
    }

    bool PluginManager::LoadPlugin(const std::string_view pluginId)
    {
        if (IsPluginLoaded(pluginId))
            return true;

        auto plan = PluginPackagePlanner{}.PlanInstall(m_Installations, m_AvailablePackages, pluginId);
        if (!plan.IsSuccessful())
        {
            LogPlanErrors(m_Logger, plan.Errors);
            return false;
        }
        return ApplyInstallations(std::move(plan.Installations), plan.LoadGroups);
    }

    bool PluginManager::UnloadPlugin(const std::string_view pluginId)
    {
        const auto installed =
            std::ranges::find(m_Installations,
                              pluginId,
                              [](const PluginInstallation& installation) { return installation.Package.Manifest.Id; });
        if (installed == m_Installations.end())
            return false;

        auto desiredExplicitIds = ExplicitPluginIds(m_Installations);
        for (const auto& installation : m_Installations)
        {
            if (!installation.IsExplicit)
                continue;
            const auto selection =
                PluginVersionSolver{}.Resolve(m_AvailablePackages, {installation.Package.Manifest.Id});
            if (selection.IsSuccessful() && ContainsPlugin(selection.Packages, pluginId))
                std::erase(desiredExplicitIds, installation.Package.Manifest.Id);
        }

        auto plan = PluginPackagePlanner{}.Plan(m_Installations, m_AvailablePackages, desiredExplicitIds);
        if (!plan.IsSuccessful())
        {
            LogPlanErrors(m_Logger, plan.Errors);
            return false;
        }
        return ApplyInstallations(std::move(plan.Installations), plan.LoadGroups);
    }

    bool PluginManager::ReloadPlugin(const std::string_view pluginId)
    {
        if (!IsPluginLoaded(pluginId))
            return false;

        const auto plan = PluginReloadPlanner{}.Plan(m_Installations, pluginId);
        if (!plan.IsSuccessful())
        {
            m_Logger.Error(PluginManagerOrigin, plan.Error, "plugins.reload.plan");
            return false;
        }

        UnloadGroups(plan.UnloadGroups);
        if (LoadGroups(m_Installations, plan.LoadGroups))
            return true;

        UnloadGroups(plan.UnloadGroups);
        std::unordered_set<std::string> affectedIds;
        for (const auto& group : plan.LoadGroups)
            affectedIds.insert(group.begin(), group.end());
        std::erase_if(m_Installations,
                      [&](const PluginInstallation& installation)
                      { return affectedIds.contains(installation.Package.Manifest.Id); });
        return false;
    }

    void PluginManager::Render()
    {
        m_ScriptEngine.AdvanceFrame();
        std::vector<std::string> failedPluginIds;
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
            const auto unloadResult = (*plugin)->Unload();
            for (const auto& diagnostic : unloadResult.Diagnostics)
                m_Logger.Error(PluginManagerOrigin,
                               "Plugin '" + unloadResult.ModuleId + "': " + DiagnosticMessage(diagnostic),
                               "plugins.script.unload." + unloadResult.ModuleId);
            failedPluginIds.push_back(result.ModuleId);
            plugin = m_LoadedPlugins.erase(plugin);
        }

        for (const auto& pluginId : failedPluginIds)
            static_cast<void>(UnloadPlugin(pluginId));
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
        m_Installations.clear();
    }

    std::size_t PluginManager::LoadedPluginCount() const noexcept
    {
        return m_LoadedPlugins.size();
    }

    bool PluginManager::ApplyInstallations(std::vector<PluginInstallation> installations,
                                           const std::vector<std::vector<std::string>>& loadGroups)
    {
        const auto previousInstallations = m_Installations;
        const auto restorePreviousInstallations = [&]
        {
            UnloadAll();
            std::vector<PluginManifest> previousManifests;
            previousManifests.reserve(previousInstallations.size());
            for (const auto& installation : previousInstallations)
                previousManifests.push_back(installation.Package.Manifest);
            const auto previousResolution = PluginDependencyResolver{}.Resolve(previousManifests);
            if (previousResolution.IsSuccessful() && LoadGroups(previousInstallations, previousResolution.LoadGroups))
                m_Installations = previousInstallations;
        };

        std::map<std::string, const PluginInstallation*> currentById;
        for (const auto& installation : m_Installations)
            currentById.emplace(installation.Package.Manifest.Id, &installation);
        std::map<std::string, const PluginInstallation*> desiredById;
        for (const auto& installation : installations)
            desiredById.emplace(installation.Package.Manifest.Id, &installation);

        const auto versionChanged =
            std::ranges::any_of(desiredById,
                                [&](const auto& desired)
                                {
                                    const auto current = currentById.find(desired.first);
                                    return current != currentById.end() && current->second->Package.Manifest.Version !=
                                                                               desired.second->Package.Manifest.Version;
                                });
        const auto hasCompatibleDependency = [](const auto& installationsById, const PluginDependency& dependency)
        {
            const auto installation = installationsById.find(dependency.Id);
            if (installation == installationsById.end())
                return false;
            const auto version = SemanticVersion::Parse(installation->second->Package.Manifest.Version);
            return version && SemanticVersionRange::Contains(dependency.VersionRange, *version);
        };
        const auto optionalDependenciesChanged = std::ranges::any_of(
            desiredById,
            [&](const auto& desired)
            {
                const auto current = currentById.find(desired.first);
                if (current == currentById.end())
                    return false;
                return std::ranges::any_of(desired.second->Package.Manifest.OptionalDependencies,
                                           [&](const PluginDependency& dependency) {
                                               return hasCompatibleDependency(currentById, dependency) !=
                                                      hasCompatibleDependency(desiredById, dependency);
                                           });
            });
        if (versionChanged || optionalDependenciesChanged)
        {
            UnloadAll();
            if (!LoadGroups(installations, loadGroups))
            {
                restorePreviousInstallations();
                return false;
            }
            m_Installations = std::move(installations);
            return true;
        }

        std::unordered_set<std::string> removedIds;
        for (const auto& [id, installation] : currentById)
        {
            static_cast<void>(installation);
            if (!desiredById.contains(id))
                removedIds.insert(id);
        }
        if (!removedIds.empty())
        {
            std::vector<PluginManifest> currentManifests;
            currentManifests.reserve(m_Installations.size());
            for (const auto& installation : m_Installations)
                currentManifests.push_back(installation.Package.Manifest);
            auto resolution = PluginDependencyResolver{}.Resolve(currentManifests);
            if (!resolution.IsSuccessful())
                return false;

            auto unloadGroups = std::move(resolution.LoadGroups);
            std::ranges::reverse(unloadGroups);
            for (auto& group : unloadGroups)
                std::erase_if(group, [&](const std::string& id) { return !removedIds.contains(id); });
            std::erase_if(unloadGroups, [](const std::vector<std::string>& group) { return group.empty(); });
            UnloadGroups(unloadGroups);
        }

        if (!LoadGroups(installations, loadGroups))
        {
            restorePreviousInstallations();
            return false;
        }
        m_Installations = std::move(installations);
        return true;
    }

    bool PluginManager::LoadGroups(const std::vector<PluginInstallation>& installations,
                                   const std::vector<std::vector<std::string>>& loadGroups)
    {
        for (const auto& loadGroup : loadGroups)
        {
            std::vector<std::unique_ptr<PluginScriptInstance>> groupInstances;
            std::vector<const PluginPackage*> groupPackages;
            for (const auto& pluginId : loadGroup)
            {
                if (IsPluginLoaded(pluginId))
                    continue;
                const auto installation = std::ranges::find(
                    installations, pluginId, [](const PluginInstallation& value) { return value.Package.Manifest.Id; });
                if (installation == installations.end())
                    return false;

                auto instance =
                    std::make_unique<PluginScriptInstance>(m_ScriptEngine,
                                                           installation->Package.Manifest,
                                                           std::filesystem::path(installation->Package.Location));
                const auto compileResult = instance->Compile(DependencyPackages(installation->Package, installations));
                if (!compileResult.IsSuccessful())
                {
                    for (const auto& diagnostic : compileResult.Diagnostics)
                        m_Logger.Error(PluginManagerOrigin,
                                       "Plugin '" + pluginId + "': " + DiagnosticMessage(diagnostic),
                                       "plugins.script.compile");
                    return false;
                }
                groupPackages.push_back(&installation->Package);
                groupInstances.push_back(std::move(instance));
            }

            for (std::size_t index{}; index < groupInstances.size(); ++index)
            {
                const auto bindResult = groupInstances[index]->BindImports();
                if (!bindResult.IsSuccessful())
                {
                    for (const auto& diagnostic : bindResult.Diagnostics)
                        m_Logger.Error(PluginManagerOrigin,
                                       "Plugin '" + groupPackages[index]->Manifest.Id +
                                           "': " + DiagnosticMessage(diagnostic),
                                       "plugins.script.bind");
                    return false;
                }
            }

            for (std::size_t index{}; index < groupInstances.size(); ++index)
            {
                const auto activation = groupInstances[index]->Activate();
                if (!activation.IsSuccessful())
                {
                    for (const auto& diagnostic : activation.Diagnostics)
                        m_Logger.Error(PluginManagerOrigin,
                                       "Plugin '" + groupPackages[index]->Manifest.Id +
                                           "': " + DiagnosticMessage(diagnostic),
                                       "plugins.script.load");
                    return false;
                }
            }

            for (std::size_t index{}; index < groupInstances.size(); ++index)
            {
                const auto& package = *groupPackages[index];
                m_Logger.Info(PluginManagerOrigin,
                              "Loaded plugin '" + package.Manifest.Name + "' " + package.Manifest.Version + ".",
                              "plugins.loaded." + package.Manifest.Id);
                m_LoadedPlugins.push_back(std::move(groupInstances[index]));
            }
        }
        return true;
    }

    void PluginManager::UnloadGroups(const std::vector<std::vector<std::string>>& unloadGroups)
    {
        for (const auto& unloadGroup : unloadGroups)
        {
            for (const auto& pluginId : unloadGroup)
            {
                const auto plugin = std::ranges::find_if(m_LoadedPlugins,
                                                         [&](const std::unique_ptr<PluginScriptInstance>& instance)
                                                         { return instance->Manifest().Id == pluginId; });
                if (plugin == m_LoadedPlugins.end())
                    continue;
                const auto result = (*plugin)->Unload();
                for (const auto& diagnostic : result.Diagnostics)
                    m_Logger.Error(PluginManagerOrigin,
                                   "Plugin '" + result.ModuleId + "': " + DiagnosticMessage(diagnostic),
                                   "plugins.script.unload." + result.ModuleId);
                m_LoadedPlugins.erase(plugin);
            }
        }
    }

    bool PluginManager::IsPluginLoaded(const std::string_view pluginId) const
    {
        return std::ranges::any_of(m_LoadedPlugins,
                                   [&](const std::unique_ptr<PluginScriptInstance>& plugin)
                                   { return plugin->Manifest().Id == pluginId; });
    }
}  // namespace PureMirror::Overlay
