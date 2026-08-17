#include "pch.h"

#include "PluginDependencyResolver.h"

#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"

#include <limits>
#include <map>
#include <tuple>
#include <unordered_set>

namespace PureMirror::Overlay
{
    namespace
    {
        struct DependencyGraph
        {
            std::vector<std::string> PluginIds;
            std::vector<std::vector<std::size_t>> Edges;
        };

        class StronglyConnectedComponentFinder
        {
          public:
            explicit StronglyConnectedComponentFinder(const DependencyGraph& graph)
                : m_Graph(graph), m_Indices(graph.PluginIds.size(), Unvisited), m_LowLinks(graph.PluginIds.size()),
                  m_OnStack(graph.PluginIds.size())
            {
            }

            std::vector<std::vector<std::string>> Find()
            {
                for (std::size_t plugin{}; plugin < m_Graph.PluginIds.size(); ++plugin)
                    if (m_Indices[plugin] == Unvisited)
                        Visit(plugin);
                return std::move(m_Groups);
            }

          private:
            void Visit(const std::size_t plugin)
            {
                m_Indices[plugin] = m_NextIndex;
                m_LowLinks[plugin] = m_NextIndex;
                ++m_NextIndex;
                m_Stack.push_back(plugin);
                m_OnStack[plugin] = true;

                for (const auto dependency : m_Graph.Edges[plugin])
                {
                    if (m_Indices[dependency] == Unvisited)
                    {
                        Visit(dependency);
                        m_LowLinks[plugin] = (std::min)(m_LowLinks[plugin], m_LowLinks[dependency]);
                    }
                    else if (m_OnStack[dependency])
                    {
                        m_LowLinks[plugin] = (std::min)(m_LowLinks[plugin], m_Indices[dependency]);
                    }
                }

                if (m_LowLinks[plugin] != m_Indices[plugin])
                    return;

                std::vector<std::string> group;
                while (true)
                {
                    const auto member = m_Stack.back();
                    m_Stack.pop_back();
                    m_OnStack[member] = false;
                    group.push_back(m_Graph.PluginIds[member]);
                    if (member == plugin)
                        break;
                }
                std::ranges::sort(group);
                m_Groups.push_back(std::move(group));
            }

            static constexpr std::size_t Unvisited = (std::numeric_limits<std::size_t>::max)();

            const DependencyGraph& m_Graph;
            std::vector<std::size_t> m_Indices;
            std::vector<std::size_t> m_LowLinks;
            std::vector<std::size_t> m_Stack;
            std::vector<bool> m_OnStack;
            std::vector<std::vector<std::string>> m_Groups;
            std::size_t m_NextIndex{};
        };
    }  // namespace

    PluginDependencyResolution PluginDependencyResolver::Resolve(const std::vector<PluginManifest>& manifests) const
    {
        PluginDependencyResolution result;
        std::map<std::string, const PluginManifest*> manifestsById;
        for (const auto& manifest : manifests)
        {
            if (!manifestsById.emplace(manifest.Id, &manifest).second)
                result.Issues.push_back({PluginDependencyIssueType::DuplicatePlugin, manifest.Id, {}});
        }

        DependencyGraph graph;
        std::unordered_map<std::string, std::size_t> indicesById;
        graph.PluginIds.reserve(manifestsById.size());
        for (const auto& [id, manifest] : manifestsById)
        {
            static_cast<void>(manifest);
            indicesById.emplace(id, graph.PluginIds.size());
            graph.PluginIds.push_back(id);
        }
        graph.Edges.resize(graph.PluginIds.size());

        for (const auto& [id, manifest] : manifestsById)
        {
            const auto plugin = indicesById.at(id);
            std::unordered_set<std::size_t> uniqueEdges;
            const auto addDependency = [&](const PluginDependency& dependencyRequirement, const bool optional)
            {
                const auto& dependencyId = dependencyRequirement.Id;
                if (dependencyId == id)
                {
                    result.Issues.push_back({PluginDependencyIssueType::SelfDependency, id, dependencyId});
                    return;
                }
                const auto dependency = indicesById.find(dependencyId);
                if (dependency == indicesById.end())
                {
                    if (!optional)
                        result.Issues.push_back({PluginDependencyIssueType::MissingDependency, id, dependencyId});
                    return;
                }
                const auto installedManifest = manifestsById.at(dependencyId);
                const auto installedVersion = SemanticVersion::Parse(installedManifest->Version);
                if (!installedVersion ||
                    !SemanticVersionRange::Contains(dependencyRequirement.VersionRange, *installedVersion))
                {
                    if (!optional)
                        result.Issues.push_back({PluginDependencyIssueType::IncompatibleVersion,
                                                 id,
                                                 dependencyId,
                                                 dependencyRequirement.VersionRange,
                                                 installedManifest->Version});
                    return;
                }
                if (uniqueEdges.insert(dependency->second).second)
                    graph.Edges[plugin].push_back(dependency->second);
            };

            for (const auto& dependency : manifest->Dependencies)
                addDependency(dependency, false);
            for (const auto& dependency : manifest->OptionalDependencies)
                addDependency(dependency, true);
            std::ranges::sort(graph.Edges[plugin]);
        }

        result.LoadGroups = StronglyConnectedComponentFinder(graph).Find();
        std::ranges::sort(result.Issues,
                          {},
                          [](const PluginDependencyIssue& issue)
                          { return std::tie(issue.PluginId, issue.DependencyId, issue.Type); });
        return result;
    }
}  // namespace PureMirror::Overlay
