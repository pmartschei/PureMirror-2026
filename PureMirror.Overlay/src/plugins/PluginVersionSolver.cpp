#include "pch.h"

#include "PluginVersionSolver.h"

#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"

#include <map>

namespace PureMirror::Overlay
{
    namespace
    {
        struct SolverState
        {
            std::map<std::string, const PluginPackage*> SelectedPackages;
            std::map<std::string, std::vector<std::string>> RequiredRanges;
        };

        struct SolverFailure
        {
            std::string PluginId;
            std::vector<std::string> RequiredRanges;
            std::size_t Depth{};
        };

        class Solver
        {
          public:
            Solver(const std::vector<PluginPackage>& availablePackages,
                   const std::vector<PluginPackage>& preferredPackages)
            {
                for (const auto& package : availablePackages)
                    if (SemanticVersion::Parse(package.Manifest.Version))
                        m_AvailablePackages[package.Manifest.Id].push_back(&package);
                for (const auto& package : preferredPackages)
                    m_PreferredVersions[package.Manifest.Id] = package.Manifest.Version;
                for (auto& [id, packages] : m_AvailablePackages)
                {
                    static_cast<void>(id);
                    std::ranges::sort(packages,
                                      [&](const PluginPackage* left, const PluginPackage* right)
                                      {
                                          const auto preferred = m_PreferredVersions.find(left->Manifest.Id);
                                          if (preferred != m_PreferredVersions.end())
                                          {
                                              const auto leftPreferred = left->Manifest.Version == preferred->second;
                                              const auto rightPreferred = right->Manifest.Version == preferred->second;
                                              if (leftPreferred != rightPreferred)
                                                  return leftPreferred;
                                          }
                                          const auto leftVersion = *SemanticVersion::Parse(left->Manifest.Version);
                                          const auto rightVersion = *SemanticVersion::Parse(right->Manifest.Version);
                                          if (leftVersion != rightVersion)
                                              return leftVersion > rightVersion;
                                          if (left->Origin != right->Origin)
                                              return left->Origin == PluginPackageOrigin::Local;
                                          return left->Location < right->Location;
                                      });
                }
            }

            bool Resolve(SolverState& state)
            {
                return Resolve(state, 0);
            }

            const SolverFailure& Failure() const noexcept
            {
                return m_Failure;
            }

          private:
            bool Resolve(SolverState& state, const std::size_t depth)
            {
                for (const auto& [id, package] : state.SelectedPackages)
                {
                    const auto version = SemanticVersion::Parse(package->Manifest.Version);
                    const auto ranges = state.RequiredRanges.find(id);
                    if (!version || ranges == state.RequiredRanges.end() || !MatchesAll(*version, ranges->second))
                    {
                        RememberFailure(id,
                                        ranges == state.RequiredRanges.end() ? std::vector<std::string>{}
                                                                             : ranges->second,
                                        depth);
                        return false;
                    }
                }

                const auto unresolved = std::ranges::find_if(
                    state.RequiredRanges,
                    [&](const auto& requirement) { return !state.SelectedPackages.contains(requirement.first); });
                if (unresolved == state.RequiredRanges.end())
                    return true;

                const auto& [pluginId, requiredRanges] = *unresolved;
                const auto available = m_AvailablePackages.find(pluginId);
                if (available == m_AvailablePackages.end())
                {
                    RememberFailure(pluginId, requiredRanges, depth);
                    return false;
                }

                bool foundCandidate = false;
                for (const auto* candidate : available->second)
                {
                    const auto version = SemanticVersion::Parse(candidate->Manifest.Version);
                    if (!version || !MatchesAll(*version, requiredRanges))
                        continue;
                    foundCandidate = true;

                    auto next = state;
                    next.SelectedPackages[pluginId] = candidate;
                    for (const auto& dependency : candidate->Manifest.Dependencies)
                        next.RequiredRanges[dependency.Id].push_back(dependency.VersionRange);
                    if (Resolve(next, depth + 1))
                    {
                        state = std::move(next);
                        return true;
                    }
                }

                if (!foundCandidate)
                    RememberFailure(pluginId, requiredRanges, depth);
                return false;
            }

            static bool MatchesAll(const SemanticVersion& version, const std::vector<std::string>& ranges)
            {
                return std::ranges::all_of(
                    ranges, [&](const auto& range) { return SemanticVersionRange::Contains(range, version); });
            }

            void RememberFailure(const std::string& pluginId,
                                 const std::vector<std::string>& requiredRanges,
                                 const std::size_t depth)
            {
                if (depth < m_Failure.Depth)
                    return;
                m_Failure = {.PluginId = pluginId, .RequiredRanges = requiredRanges, .Depth = depth};
            }

            std::map<std::string, std::vector<const PluginPackage*>> m_AvailablePackages;
            std::map<std::string, std::string> m_PreferredVersions;
            SolverFailure m_Failure;
        };
    }  // namespace

    PluginVersionSelectionResult PluginVersionSolver::Resolve(const std::vector<PluginPackage>& availablePackages,
                                                              const std::vector<std::string>& rootPluginIds,
                                                              const std::vector<PluginPackage>& preferredPackages) const
    {
        SolverState state;
        for (const auto& pluginId : rootPluginIds)
            state.RequiredRanges[pluginId].push_back(">=0.0.0");

        Solver solver(availablePackages, preferredPackages);
        if (!solver.Resolve(state))
        {
            const auto& failure = solver.Failure();
            return {.Errors = {{.PluginId = failure.PluginId,
                                .RequiredRanges = failure.RequiredRanges,
                                .Message = "No available version satisfies all required ranges."}}};
        }

        PluginVersionSelectionResult result;
        result.Packages.reserve(state.SelectedPackages.size());
        for (const auto& [id, package] : state.SelectedPackages)
        {
            static_cast<void>(id);
            result.Packages.push_back(*package);
        }
        return result;
    }
}  // namespace PureMirror::Overlay
