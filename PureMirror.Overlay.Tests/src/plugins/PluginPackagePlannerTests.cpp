#include "CppUnitTest.h"
#include "src/plugins/PluginPackagePlanner.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        PluginPackage PlannerPackage(std::string id,
                                     std::string version,
                                     std::vector<PluginDependency> dependencies = {},
                                     const PluginPackageOrigin origin = PluginPackageOrigin::Local)
        {
            const auto location = id + '-' + version;
            return {.Manifest = {.Id = std::move(id),
                                 .Version = std::move(version),
                                 .Dependencies = std::move(dependencies)},
                    .Origin = origin,
                    .Location = location};
        }

        bool HasChange(const PluginChangePlan& plan, const PluginChangeType type, const std::string_view pluginId)
        {
            return std::ranges::any_of(plan.Changes,
                                       [&](const PluginChange& change)
                                       { return change.Type == type && change.PluginId == pluginId; });
        }
    }  // namespace

    TEST_CLASS(PluginPackagePlannerTests)
    {
      public:
        TEST_METHOD(PlanInstall_AddsRemoteDependencyAndUpdatesSharedVersionAtomically)
        {
            const auto pluginA = PlannerPackage("plugin-a", "1.0.0", {{"shared", ">=1.0.0 <2.0.0"}});
            const auto shared15 = PlannerPackage("shared", "1.5.0");
            const std::vector current{PluginInstallation{pluginA, true}, PluginInstallation{shared15, false}};
            const std::vector available{pluginA,
                                        shared15,
                                        PlannerPackage("plugin-c", "1.0.0", {{"shared", ">=1.7.0 <2.0.0"}}),
                                        PlannerPackage("shared", "1.7.0", {}, PluginPackageOrigin::Remote)};

            const auto plan = PluginPackagePlanner{}.PlanInstall(current, available, "plugin-c");

            Assert::IsTrue(plan.IsSuccessful());
            Assert::IsTrue(HasChange(plan, PluginChangeType::Install, "plugin-c"));
            Assert::IsTrue(HasChange(plan, PluginChangeType::Update, "shared"));
        }

        TEST_METHOD(PlanRemove_RemovesOrphanedTransitiveDependency)
        {
            const auto pluginA = PlannerPackage("plugin-a", "1.0.0", {{"shared", ">=1.0.0"}});
            const auto shared = PlannerPackage("shared", "1.5.0");
            const std::vector current{PluginInstallation{pluginA, true}, PluginInstallation{shared, false}};

            const auto plan = PluginPackagePlanner{}.PlanRemove(current, {pluginA, shared}, "plugin-a");

            Assert::IsTrue(plan.IsSuccessful());
            Assert::IsTrue(HasChange(plan, PluginChangeType::Remove, "plugin-a"));
            Assert::IsTrue(HasChange(plan, PluginChangeType::Remove, "shared"));
        }

        TEST_METHOD(PlanRemove_KeepsDependencyNeededByAnotherExplicitPlugin)
        {
            const auto pluginA = PlannerPackage("plugin-a", "1.0.0", {{"shared", ">=1.0.0"}});
            const auto pluginC = PlannerPackage("plugin-c", "1.0.0", {{"shared", ">=1.0.0"}});
            const auto shared = PlannerPackage("shared", "1.5.0");
            const std::vector current{PluginInstallation{pluginA, true},
                                      PluginInstallation{pluginC, true},
                                      PluginInstallation{shared, false}};

            const auto plan = PluginPackagePlanner{}.PlanRemove(current, {pluginA, pluginC, shared}, "plugin-c");

            Assert::IsTrue(plan.IsSuccessful());
            Assert::IsTrue(HasChange(plan, PluginChangeType::Remove, "plugin-c"));
            Assert::IsFalse(HasChange(plan, PluginChangeType::Remove, "shared"));
        }

        TEST_METHOD(PlanRemove_RejectsDirectRemovalOfRequiredDependency)
        {
            const auto pluginA = PlannerPackage("plugin-a", "1.0.0", {{"shared", ">=1.0.0"}});
            const auto shared = PlannerPackage("shared", "1.5.0");
            const std::vector current{PluginInstallation{pluginA, true}, PluginInstallation{shared, false}};

            const auto plan = PluginPackagePlanner{}.PlanRemove(current, {pluginA, shared}, "shared");

            Assert::IsFalse(plan.IsSuccessful());
            Assert::AreEqual(std::string{"shared"}, plan.Errors.front().PluginId);
        }
    };
}  // namespace PureMirror::Overlay::Tests
