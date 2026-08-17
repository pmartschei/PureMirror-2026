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
        TEST_METHOD(Compile_LoadsManifestEntryAndAllExportsAsOneModule)
        {
            TemporaryPluginDirectory package;
            package.Write("scripts/main.as", "void on_load() { exported_helper(); }");
            package.Write("scripts/exports/helper.as", "void exported_helper() {}");
            const PluginManifest manifest{
                .Id = "com.example.files", .Entry = "scripts/main.as", .Exports = {"scripts/exports/helper.as"}};
            AngelScriptEngine engine;

            const auto result = PluginScriptCompiler(engine).Compile(manifest, package.Path());

            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::string{"com.example.files"}, result.ModuleId);
        }

        TEST_METHOD(Compile_ReportsMissingExportWithoutReplacingExistingModule)
        {
            TemporaryPluginDirectory package;
            package.Write("scripts/main.as", "void on_load() {}");
            const PluginManifest manifest{
                .Id = "com.example.missing", .Entry = "scripts/main.as", .Exports = {"scripts/exports/missing.as"}};
            AngelScriptEngine engine;

            const auto result = PluginScriptCompiler(engine).Compile(manifest, package.Path());

            Assert::IsFalse(result.IsSuccessful());
            Assert::AreEqual(std::string{"scripts/exports/missing.as"}, result.Diagnostics.front().Section);
        }
    };
}  // namespace PureMirror::Overlay::Tests
