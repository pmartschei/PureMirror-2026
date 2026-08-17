#include "CppUnitTest.h"
#include "src/plugins/PluginVersionSolver.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        PluginPackage Package(std::string id,
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

        const PluginPackage* Find(const PluginVersionSelectionResult& result, const std::string_view id)
        {
            const auto package = std::ranges::find_if(
                result.Packages, [&](const PluginPackage& candidate) { return candidate.Manifest.Id == id; });
            return package == result.Packages.end() ? nullptr : &*package;
        }
    }  // namespace

    TEST_CLASS(PluginVersionSolverTests)
    {
      public:
        TEST_METHOD(Resolve_SelectsHighestVersionThatSatisfiesAllPlugins)
        {
            const std::vector packages{Package("plugin-a", "1.0.0", {{"shared", ">=1.5.0 <2.0.0"}}),
                                       Package("plugin-b", "1.0.0", {{"shared", ">=1.7.0 <1.8.0"}}),
                                       Package("shared", "1.5.0"),
                                       Package("shared", "1.7.0"),
                                       Package("shared", "2.0.0")};

            const auto result = PluginVersionSolver{}.Resolve(packages, {"plugin-a", "plugin-b"});

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::string{"1.7.0"}, Find(result, "shared")->Manifest.Version);
        }

        TEST_METHOD(Resolve_PrefersInstalledVersionWhileItRemainsCompatible)
        {
            const auto installed = Package("shared", "1.5.0");
            const std::vector packages{
                Package("plugin-a", "1.0.0", {{"shared", ">=1.0.0 <2.0.0"}}), installed, Package("shared", "1.7.0")};

            const auto result = PluginVersionSolver{}.Resolve(packages, {"plugin-a"}, {installed});

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::string{"1.5.0"}, Find(result, "shared")->Manifest.Version);
        }

        TEST_METHOD(Resolve_BacktracksFromNewestPluginVersionWhenDependenciesConflict)
        {
            const std::vector packages{Package("plugin-a", "2.0.0", {{"shared", ">=2.0.0"}}),
                                       Package("plugin-a", "1.0.0", {{"shared", ">=1.0.0 <2.0.0"}}),
                                       Package("plugin-b", "1.0.0", {{"shared", "<2.0.0"}}),
                                       Package("shared", "1.5.0"),
                                       Package("shared", "2.0.0")};

            const auto result = PluginVersionSolver{}.Resolve(packages, {"plugin-a", "plugin-b"});

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::string{"1.0.0"}, Find(result, "plugin-a")->Manifest.Version);
            Assert::AreEqual(std::string{"1.5.0"}, Find(result, "shared")->Manifest.Version);
        }

        TEST_METHOD(Resolve_ReportsConflictWhenNoSharedVersionExists)
        {
            const std::vector packages{Package("plugin-a", "1.0.0", {{"shared", "=1.5.0"}}),
                                       Package("plugin-b", "1.0.0", {{"shared", "=1.7.0"}}),
                                       Package("shared", "1.5.0"),
                                       Package("shared", "1.7.0")};

            const auto result = PluginVersionSolver{}.Resolve(packages, {"plugin-a", "plugin-b"});

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::string{"shared"}, result.Errors.front().PluginId);
        }
    };
}  // namespace PureMirror::Overlay::Tests
