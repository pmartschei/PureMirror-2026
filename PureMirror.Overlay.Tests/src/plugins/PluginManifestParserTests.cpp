#include "CppUnitTest.h"
#include "src/plugins/PluginManifestParser.h"

#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(PluginManifestParserTests)
    {
      public:
        TEST_METHOD(Parse_ReadsExportsAndDependencies)
        {
            constexpr auto json = R"({
                "schemaVersion": 1,
                "id": "com.example.consumer",
                "name": "Consumer",
                "version": "0.1.0",
                "apiVersion": "1.0",
                "entry": "scripts/main.as",
                "exports": ["scripts/public/one.as", "scripts/public/two.as"],
                "dependencies": {"com.example.required": ">=1.2.0 <2.0.0"},
                "optionalDependencies": {"com.example.optional": "=3.0.0"},
                "capabilities": ["ui"]
            })";

            const auto result = PluginManifestParser{}.Parse(json);

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, result.Manifest.Exports.size());
            Assert::AreEqual(std::string{"com.example.required"}, result.Manifest.Dependencies.front().Id);
            Assert::AreEqual(std::string{">=1.2.0 <2.0.0"}, result.Manifest.Dependencies.front().VersionRange);
            Assert::AreEqual(std::string{"com.example.optional"}, result.Manifest.OptionalDependencies.front().Id);
        }

        TEST_METHOD(Parse_RejectsSelfDependencyAndUnsafeExport)
        {
            constexpr auto json = R"({
                "schemaVersion": 1,
                "id": "com.example.self",
                "name": "Self",
                "version": "0.1.0",
                "apiVersion": "1.0",
                "entry": "scripts/main.as",
                "exports": ["../outside.as"],
                "dependencies": {"com.example.self": ">=1.0.0"}
            })";

            const auto result = PluginManifestParser{}.Parse(json);

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, result.Errors.size());
        }

        TEST_METHOD(Parse_RejectsInvalidPluginVersionAndDependencyRange)
        {
            constexpr auto json = R"({
                "schemaVersion": 1,
                "id": "com.example.invalid-version",
                "name": "Invalid version",
                "version": "1.0",
                "apiVersion": "1.0",
                "entry": "scripts/main.as",
                "dependencies": {"com.example.provider": "^1.0.0"}
            })";

            const auto result = PluginManifestParser{}.Parse(json);

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, result.Errors.size());
        }

        TEST_METHOD(Parse_RejectsTrailingComma)
        {
            constexpr auto json = R"({
                "schemaVersion": 1,
                "id": "com.example.invalid",
            })";

            const auto result = PluginManifestParser{}.Parse(json);

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::string{"$"}, result.Errors.front().Field);
        }
    };
}  // namespace PureMirror::Overlay::Tests
