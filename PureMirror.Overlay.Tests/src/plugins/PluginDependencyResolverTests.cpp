#include "CppUnitTest.h"
#include "src/plugins/PluginDependencyResolver.h"

#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        PluginManifest Manifest(std::string id,
                                std::vector<PluginDependency> dependencies = {},
                                std::vector<PluginDependency> optionalDependencies = {},
                                std::string version = "1.0.0")
        {
            return {.Id = std::move(id),
                    .Version = std::move(version),
                    .Dependencies = std::move(dependencies),
                    .OptionalDependencies = std::move(optionalDependencies)};
        }
    }  // namespace

    TEST_CLASS(PluginDependencyResolverTests)
    {
      public:
        TEST_METHOD(Resolve_AllowsRequiredCycleAndPlacesConsumerAfterIt)
        {
            const std::vector manifests{Manifest("com.example.a", {{"com.example.b", ">=1.0.0"}}),
                                        Manifest("com.example.b", {{"com.example.a", ">=1.0.0"}}),
                                        Manifest("com.example.consumer", {{"com.example.a", "<2.0.0"}})};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, result.LoadGroups.size());
            Assert::AreEqual(std::size_t{2}, result.LoadGroups[0].size());
            Assert::AreEqual(std::string{"com.example.a"}, result.LoadGroups[0][0]);
            Assert::AreEqual(std::string{"com.example.b"}, result.LoadGroups[0][1]);
            Assert::AreEqual(std::string{"com.example.consumer"}, result.LoadGroups[1][0]);
        }

        TEST_METHOD(Resolve_AllowsOptionalCycleWhenBothPluginsExist)
        {
            const std::vector manifests{Manifest("com.example.a", {}, {{"com.example.b", "=1.0.0"}}),
                                        Manifest("com.example.b", {}, {{"com.example.a", "=1.0.0"}})};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{1}, result.LoadGroups.size());
            Assert::AreEqual(std::size_t{2}, result.LoadGroups.front().size());
        }

        TEST_METHOD(Resolve_IgnoresMissingOptionalDependency)
        {
            const std::vector manifests{Manifest("com.example.a", {}, {{"com.example.not-installed", ">=1.0.0"}})};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{1}, result.LoadGroups.size());
        }

        TEST_METHOD(Resolve_ReportsMissingRequiredDependency)
        {
            const std::vector manifests{Manifest("com.example.a", {{"com.example.not-installed", ">=1.0.0"}})};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::size_t{1}, result.Issues.size());
            Assert::IsTrue(result.Issues.front().Type == PluginDependencyIssueType::MissingDependency);
            Assert::AreEqual(std::string{"com.example.not-installed"}, result.Issues.front().DependencyId);
        }

        TEST_METHOD(Resolve_ReportsIncompatibleRequiredVersion)
        {
            const std::vector manifests{Manifest("com.example.consumer", {{"com.example.provider", ">=2.0.0"}}),
                                        Manifest("com.example.provider", {}, {}, "1.5.0")};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::size_t{1}, result.Issues.size());
            Assert::IsTrue(result.Issues.front().Type == PluginDependencyIssueType::IncompatibleVersion);
            Assert::AreEqual(std::string{">=2.0.0"}, result.Issues.front().RequiredVersion);
            Assert::AreEqual(std::string{"1.5.0"}, result.Issues.front().InstalledVersion);
        }

        TEST_METHOD(Resolve_IgnoresIncompatibleOptionalVersion)
        {
            const std::vector manifests{Manifest("com.example.consumer", {}, {{"com.example.provider", ">=2.0.0"}}),
                                        Manifest("com.example.provider", {}, {}, "1.5.0")};

            const auto result = PluginDependencyResolver{}.Resolve(manifests);

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, result.LoadGroups.size());
        }
    };
}  // namespace PureMirror::Overlay::Tests
