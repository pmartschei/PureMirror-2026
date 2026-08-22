#include "CppUnitTest.h"
#include "src/scripting/PluginScriptCompiler.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        class TemporaryPluginDirectory
        {
          public:
            TemporaryPluginDirectory()
            {
                const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
                m_Path = std::filesystem::temp_directory_path() /
                         ("PureMirror.PluginScriptCompiler." + std::to_string(suffix));
                std::filesystem::create_directories(m_Path / "scripts/exports");
            }

            ~TemporaryPluginDirectory()
            {
                std::error_code error;
                std::filesystem::remove_all(m_Path, error);
            }

            const std::filesystem::path& Path() const noexcept
            {
                return m_Path;
            }

            void Write(const std::filesystem::path& relativePath, const std::string_view content) const
            {
                std::ofstream stream(m_Path / relativePath, std::ios::binary);
                stream << content;
            }

          private:
            std::filesystem::path m_Path;
        };
    }  // namespace

    TEST_CLASS(PluginScriptCompilerTests)
    {
      public:
        TEST_METHOD(Compile_ExcludesProvidingPluginsOwnExportDeclarations)
        {
            TemporaryPluginDirectory package;
            package.Write("scripts/main.as", "void exported_helper() {} void OnLoad() { exported_helper(); }");
            package.Write("scripts/exports/helper.as", "this is intentionally not valid AngelScript");
            const PluginManifest manifest{
                .Id = "com.example.files", .Entry = "scripts/main.as", .Exports = {"scripts/exports/helper.as"}};
            AngelScriptEngine engine;

            const auto result = PluginScriptCompiler(engine).Compile(manifest, package.Path());

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::string{"com.example.files"}, result.ModuleId);
        }

        TEST_METHOD(Compile_IncludesDependencyExportDeclarations)
        {
            TemporaryPluginDirectory provider;
            provider.Write("scripts/main.as", "int exported_value() { return 42; }");
            provider.Write("scripts/exports/helper.as", "import int exported_value() from \"com.example.provider\";");
            const PluginManifest providerManifest{
                .Id = "com.example.provider", .Entry = "scripts/main.as", .Exports = {"scripts/exports/helper.as"}};

            TemporaryPluginDirectory consumer;
            consumer.Write("scripts/main.as", "void OnLoad() { exported_value(); }");
            const PluginManifest consumerManifest{.Id = "com.example.consumer", .Entry = "scripts/main.as"};
            const PluginPackage providerPackage{.Manifest = providerManifest, .Location = provider.Path().string()};
            AngelScriptEngine engine;

            const auto providerResult = PluginScriptCompiler(engine).Compile(providerManifest, provider.Path());
            const auto providerBindings = engine.BindModuleImports(providerManifest.Id);
            const auto consumerResult =
                PluginScriptCompiler(engine).Compile(consumerManifest, consumer.Path(), {providerPackage});
            const auto consumerBindings = engine.BindModuleImports(consumerManifest.Id);
            const auto callback = engine.CallFunction(consumerManifest.Id, {"void OnLoad()"});

            Assert::IsTrue(providerResult.IsSuccessful());
            Assert::IsTrue(providerBindings.IsSuccessful());
            Assert::IsTrue(consumerResult.IsSuccessful());
            Assert::IsTrue(consumerBindings.IsSuccessful());
            Assert::IsTrue(callback.IsSuccessful());
        }

        TEST_METHOD(Compile_ReportsMissingDependencyExport)
        {
            TemporaryPluginDirectory provider;
            provider.Write("scripts/main.as", "void exported_helper() {}");
            const PluginManifest providerManifest{
                .Id = "com.example.provider", .Entry = "scripts/main.as", .Exports = {"scripts/exports/missing.as"}};
            TemporaryPluginDirectory consumer;
            consumer.Write("scripts/main.as", "void OnLoad() {}");
            const PluginManifest consumerManifest{.Id = "com.example.consumer", .Entry = "scripts/main.as"};
            const PluginPackage providerPackage{.Manifest = providerManifest, .Location = provider.Path().string()};
            AngelScriptEngine engine;

            const auto result =
                PluginScriptCompiler(engine).Compile(consumerManifest, consumer.Path(), {providerPackage});

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::string{"com.example.provider/scripts/exports/missing.as"},
                             result.Diagnostics.front().Section);
        }
    };
}  // namespace PureMirror::Overlay::Tests
