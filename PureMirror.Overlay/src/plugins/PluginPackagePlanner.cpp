#include "pch.h"

#include "PluginPackagePlanner.h"

#include "PluginDependencyResolver.h"
#include "PluginVersionSolver.h"

#include <map>
#include <unordered_set>

namespace PureMirror::Overlay
{
    namespace
    {
        std::vector<std::string> ExplicitPluginIds(const std::vector<PluginInstallation>& installations)
        {
            std::vector<std::string> result;
            for (const auto& installation : installations)
                if (installation.IsExplicit)
                    result.push_back(installation.Package.Manifest.Id);
            return result;
        }

        std::vector<PluginPackage> InstalledPackages(const std::vector<PluginInstallation>& installations)
        {
            std::vector<PluginPackage> result;
            result.reserve(installations.size());
            for (const auto& installation : installations)
                result.push_back(installation.Package);
            return result;
        }
    }  // namespace

    PluginChangePlan PluginPackagePlanner::Plan(const std::vector<PluginInstallation>& currentInstallations,
                                                const std::vector<PluginPackage>& availablePackages,
                                                const std::vector<std::string>& desiredExplicitPluginIds) const
    {
        auto catalog = availablePackages;
        const auto installedPackages = InstalledPackages(currentInstallations);
        catalog.insert(catalog.end(), installedPackages.begin(), installedPackages.end());

        const auto selection = PluginVersionSolver{}.Resolve(catalog, desiredExplicitPluginIds, installedPackages);
        if (!selection.IsSuccessful())
            return {.Errors = selection.Errors};

        std::vector<PluginManifest> manifests;
        manifests.reserve(selection.Packages.size());
        for (const auto& package : selection.Packages)
            manifests.push_back(package.Manifest);
        const auto dependencyResolution = PluginDependencyResolver{}.Resolve(manifests);
        if (!dependencyResolution.IsSuccessful())
        {
            PluginChangePlan result;
            for (const auto& issue : dependencyResolution.Issues)
                result.Errors.push_back(
                    {.PluginId = issue.PluginId,
                     .RequiredRanges = {issue.RequiredVersion},
                     .Message = "Selected packages have an unresolved dependency on '" + issue.DependencyId + "'."});
            return result;
        }

        const std::unordered_set<std::string> explicitIds(desiredExplicitPluginIds.begin(),
                                                          desiredExplicitPluginIds.end());
        std::map<std::string, const PluginInstallation*> currentById;
        for (const auto& installation : currentInstallations)
            currentById[installation.Package.Manifest.Id] = &installation;
        std::map<std::string, const PluginPackage*> selectedById;
        for (const auto& package : selection.Packages)
            selectedById[package.Manifest.Id] = &package;

        PluginChangePlan result;
        result.LoadGroups = dependencyResolution.LoadGroups;
        result.Installations.reserve(selection.Packages.size());
        for (const auto& [id, package] : selectedById)
        {
            result.Installations.push_back({.Package = *package, .IsExplicit = explicitIds.contains(id)});
            const auto current = currentById.find(id);
            if (current == currentById.end())
            {
                result.Changes.push_back({.Type = PluginChangeType::Install,
                                          .PluginId = id,
                                          .ToVersion = package->Manifest.Version,
                                          .Package = *package});
            }
            else if (current->second->Package.Manifest.Version != package->Manifest.Version)
            {
                result.Changes.push_back({.Type = PluginChangeType::Update,
                                          .PluginId = id,
                                          .FromVersion = current->second->Package.Manifest.Version,
                                          .ToVersion = package->Manifest.Version,
                                          .Package = *package});
            }
        }
        for (const auto& [id, installation] : currentById)
        {
            if (!selectedById.contains(id))
                result.Changes.push_back({.Type = PluginChangeType::Remove,
                                          .PluginId = id,
                                          .FromVersion = installation->Package.Manifest.Version,
                                          .Package = installation->Package});
        }
        return result;
    }

    PluginChangePlan PluginPackagePlanner::PlanInstall(const std::vector<PluginInstallation>& currentInstallations,
                                                       const std::vector<PluginPackage>& availablePackages,
                                                       const std::string_view pluginId) const
    {
        auto explicitIds = ExplicitPluginIds(currentInstallations);
        if (std::ranges::find(explicitIds, pluginId) == explicitIds.end())
            explicitIds.emplace_back(pluginId);
        return Plan(currentInstallations, availablePackages, explicitIds);
    }

    PluginChangePlan PluginPackagePlanner::PlanRemove(const std::vector<PluginInstallation>& currentInstallations,
                                                      const std::vector<PluginPackage>& availablePackages,
                                                      const std::string_view pluginId) const
    {
        auto explicitIds = ExplicitPluginIds(currentInstallations);
        std::erase(explicitIds, pluginId);
        auto result = Plan(currentInstallations, availablePackages, explicitIds);
        if (!result.IsSuccessful())
            return result;

        const auto remaining = std::ranges::find_if(result.Installations,
                                                    [&](const PluginInstallation& installation)
                                                    { return installation.Package.Manifest.Id == pluginId; });
        if (remaining != result.Installations.end() && !remaining->IsExplicit)
            result.Errors.push_back({.PluginId = std::string(pluginId),
                                     .Message = "Plugin is still required by another installed plugin."});
        return result;
    }
}  // namespace PureMirror::Overlay
