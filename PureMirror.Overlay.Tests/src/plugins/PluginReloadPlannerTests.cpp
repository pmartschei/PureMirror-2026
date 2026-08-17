#include "CppUnitTest.h"
#include "src/plugins/PluginReloadPlanner.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        PluginInstallation ReloadInstallation(std::string id, std::vector<PluginDependency> dependencies = {})
        {
            return {.Package = {.Manifest = {
                                    .Id = std::move(id), .Version = "1.0.0", .Dependencies = std::move(dependencies)}}};
        }
    }  // namespace

    TEST_CLASS(PluginReloadPlannerTests)
    {
      public:
        TEST_METHOD(Plan_ReloadsDependentsAndLeavesUnrelatedPluginRunning)
        {
            const std::vector installations{ReloadInstallation("plugin-a", {{"shared", ">=1.0.0"}}),
                                            ReloadInstallation("plugin-c", {{"shared", ">=1.0.0"}}),
                                            ReloadInstallation("shared"),
                                            ReloadInstallation("unrelated")};

            const auto plan = PluginReloadPlanner{}.Plan(installations, "shared");

            Assert::IsTrue(plan.IsSuccessful());
            Assert::AreEqual(std::size_t{3}, plan.LoadGroups.size());
            Assert::AreEqual(std::string{"shared"}, plan.LoadGroups.front().front());
            Assert::AreEqual(std::string{"shared"}, plan.UnloadGroups.back().front());
            for (const auto& group : plan.LoadGroups)
                Assert::IsFalse(std::ranges::find(group, "unrelated") != group.end());
        }

        TEST_METHOD(Plan_ReloadsEntireCyclicGroup)
        {
            const std::vector installations{ReloadInstallation("plugin-a", {{"plugin-b", ">=1.0.0"}}),
                                            ReloadInstallation("plugin-b", {{"plugin-a", ">=1.0.0"}})};

            const auto plan = PluginReloadPlanner{}.Plan(installations, "plugin-a");

            Assert::IsTrue(plan.IsSuccessful());
            Assert::AreEqual(std::size_t{1}, plan.LoadGroups.size());
            Assert::AreEqual(std::size_t{2}, plan.LoadGroups.front().size());
        }
    };
}  // namespace PureMirror::Overlay::Tests
