#include "pch.h"

#include "PluginReloadPlanner.h"

#include "PluginDependencyResolver.h"
#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"

#include <map>
#include <unordered_set>

namespace PureMirror::Overlay
{
    PluginReloadPlan PluginReloadPlanner::Plan(const std::vector<PluginInstallation>& installations,
                                               const std::string_view pluginId) const
    {
        const std::string targetPluginId(pluginId);
        std::map<std::string, const PluginManifest*> manifestsById;
        std::vector<PluginManifest> manifests;
        manifests.reserve(installations.size());
        for (const auto& installation : installations)
        {
            manifests.push_back(installation.Package.Manifest);
            manifestsById[installation.Package.Manifest.Id] = &installation.Package.Manifest;
        }
        if (!manifestsById.contains(targetPluginId))
            return {.Error = "Plugin is not installed."};

        const auto resolution = PluginDependencyResolver{}.Resolve(manifests);
        if (!resolution.IsSuccessful())
            return {.Error = "Installed plugins do not have a valid dependency resolution."};

        std::unordered_set<std::string> affected{targetPluginId};
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& manifest : manifests)
            {
                if (affected.contains(manifest.Id))
                    continue;
                const auto dependsOnAffected = [&](const PluginDependency& dependency)
                {
                    if (!affected.contains(dependency.Id))
                        return false;
                    const auto installed = manifestsById.find(dependency.Id);
                    if (installed == manifestsById.end())
                        return false;
                    const auto version = SemanticVersion::Parse(installed->second->Version);
                    return version && SemanticVersionRange::Contains(dependency.VersionRange, *version);
                };
                if (std::ranges::any_of(manifest.Dependencies, dependsOnAffected) ||
                    std::ranges::any_of(manifest.OptionalDependencies, dependsOnAffected))
                {
                    affected.insert(manifest.Id);
                    changed = true;
                }
            }
        }

        PluginReloadPlan result;
        for (const auto& group : resolution.LoadGroups)
        {
            if (std::ranges::any_of(group, [&](const std::string& id) { return affected.contains(id); }))
                result.LoadGroups.push_back(group);
        }
        result.UnloadGroups = result.LoadGroups;
        std::ranges::reverse(result.UnloadGroups);
        return result;
    }
}  // namespace PureMirror::Overlay
