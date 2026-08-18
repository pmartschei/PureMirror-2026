#include "CppUnitTest.h"
#include "src/core/logger/Logger.h"
#include "src/plugins/PluginManager.h"
#include "src/scripting/IScriptHost.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        class PluginManagerScriptHost final : public IScriptHost
        {
          public:
            void LogInfo(std::string_view pluginId, std::string_view message) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(message);
            }

            bool BeginWindow(std::string_view pluginId, std::string_view title) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(title);
                return true;
            }

            void EndWindow(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
            }

            void Text(std::string_view pluginId, std::string_view value) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(value);
            }

            bool Button(std::string_view pluginId, std::string_view label) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(label);
                return false;
            }
        };

        std::filesystem::path ExamplePluginsRoot()
        {
            auto repositoryRoot = std::filesystem::path(__FILE__).parent_path();
            for (int depth = 0; depth < 3; ++depth)
                repositoryRoot = repositoryRoot.parent_path();
            return repositoryRoot / "PureMirror.Overlay/examples/plugins";
        }

        void CreatePlugin(const std::filesystem::path& pluginsRoot,
                          const std::string_view id,
                          const std::string_view name,
                          const std::string_view dependencies = {})
        {
            const auto pluginRoot = pluginsRoot / id;
            std::filesystem::create_directories(pluginRoot / "scripts");
            std::ofstream(pluginRoot / "plugin.json") << "{\n"
                                                         "  \"schemaVersion\": 1,\n"
                                                         "  \"id\": \""
                                                      << id
                                                      << "\",\n"
                                                         "  \"name\": \""
                                                      << name
                                                      << "\",\n"
                                                         "  \"version\": \"1.0.0\",\n"
                                                         "  \"apiVersion\": \"1.0\",\n"
                                                         "  \"entry\": \"scripts/main.as\",\n"
                                                         "  \"exports\": [],\n"
                                                         "  \"dependencies\": {"
                                                      << dependencies
                                                      << "},\n"
                                                         "  \"optionalDependencies\": {},\n"
                                                         "  \"capabilities\": []\n"
                                                         "}\n";
            std::ofstream(pluginRoot / "scripts/main.as") << "void on_load() {}\n"
                                                             "void on_render() {}\n"
                                                             "void on_unload() {}\n";
        }

        void CreateOptionalExportPlugins(const std::filesystem::path& pluginsRoot)
        {
            CreatePlugin(pluginsRoot, "com.test.optional-provider", "Optional Provider");
            const auto providerRoot = pluginsRoot / "com.test.optional-provider";
            std::filesystem::create_directories(providerRoot / "scripts/exports");
            std::ofstream(providerRoot / "plugin.json") << R"({
  "schemaVersion": 1,
  "id": "com.test.optional-provider",
  "name": "Optional Provider",
  "version": "1.0.0",
  "apiVersion": "1.0",
  "entry": "scripts/main.as",
  "exports": ["scripts/exports/api.as"],
  "dependencies": {},
  "optionalDependencies": {},
  "capabilities": []
})";
            std::ofstream(providerRoot / "scripts/main.as") << "int optional_value() { return 42; }\n"
                                                               "void on_load() {}\n"
                                                               "void on_render() {}\n"
                                                               "void on_unload() {}\n";
            std::ofstream(providerRoot / "scripts/exports/api.as")
                << "import int optional_value() from \"com.test.optional-provider\";\n";

            CreatePlugin(pluginsRoot, "com.test.optional-consumer", "Optional Consumer");
            const auto consumerRoot = pluginsRoot / "com.test.optional-consumer";
            std::ofstream(consumerRoot / "plugin.json") << R"({
  "schemaVersion": 1,
  "id": "com.test.optional-consumer",
  "name": "Optional Consumer",
  "version": "1.0.0",
  "apiVersion": "1.0",
  "entry": "scripts/main.as",
  "exports": [],
  "dependencies": {},
  "optionalDependencies": {"com.test.optional-provider": ">=1.0.0"},
  "capabilities": []
})";
            std::ofstream(consumerRoot / "scripts/main.as") << "void on_load() {}\n"
                                                               "void on_render() { optional_value(); }\n"
                                                               "void on_unload() {}\n";
        }

        bool ContainsPlugin(const std::vector<PluginInfo>& plugins, const std::string_view pluginId)
        {
            return std::ranges::find(plugins, pluginId, &PluginInfo::Id) != plugins.end();
        }
    }  // namespace

    TEST_CLASS(PluginManagerTests)
    {
      public:
        TEST_METHOD(ScanPlugins_DiscoversExampleWithoutLoadingIt)
        {
            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            const auto discoveredCount = manager.ScanPlugins(ExamplePluginsRoot());

            Assert::AreEqual(std::size_t{5}, discoveredCount);
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());
            Assert::AreEqual(std::size_t{5}, manager.AvailablePlugins().size());

            Assert::IsTrue(manager.LoadPlugin("com.puremirror.example.hello-overlay"));
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());
            manager.Render();
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());

            Assert::IsTrue(manager.UnloadPlugin("com.puremirror.example.hello-overlay"));
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());
        }

        TEST_METHOD(ExamplePlugins_LoadDependencyAndCyclicGroups)
        {
            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{5}, manager.ScanPlugins(ExamplePluginsRoot()));

            Assert::IsTrue(manager.LoadPlugin("com.puremirror.example.dependency-consumer"));
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.puremirror.example.dependency-consumer"));
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.puremirror.example.dependency-shared"));
            manager.Render();
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());
            manager.UnloadAll();

            Assert::IsTrue(manager.LoadPlugin("com.puremirror.example.cyclic-a"));
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.puremirror.example.cyclic-a"));
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.puremirror.example.cyclic-b"));
        }

        TEST_METHOD(PluginActions_LoadReloadAndUnloadDependencyClosure)
        {
            const auto pluginsRoot = std::filesystem::temp_directory_path() / "PureMirror.PluginManagerTests";
            std::error_code error;
            std::filesystem::remove_all(pluginsRoot, error);
            CreatePlugin(pluginsRoot, "com.test.shared", "Shared");
            CreatePlugin(pluginsRoot, "com.test.plugin-a", "Plugin A", "\"com.test.shared\": \">=1.0.0\"");
            CreatePlugin(pluginsRoot, "com.test.plugin-b", "Plugin B", "\"com.test.shared\": \">=1.0.0\"");

            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{3}, manager.ScanPlugins(pluginsRoot));
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());

            Assert::IsTrue(manager.LoadPlugin("com.test.plugin-a"));
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());
            Assert::AreEqual(std::size_t{1}, manager.AvailablePlugins().size());
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.test.shared"));

            Assert::IsTrue(manager.LoadPlugin("com.test.plugin-b"));
            Assert::AreEqual(std::size_t{3}, manager.LoadedPluginCount());

            Assert::IsTrue(manager.UnloadPlugin("com.test.plugin-a"));
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.test.plugin-b"));
            Assert::IsTrue(ContainsPlugin(manager.LoadedPlugins(), "com.test.shared"));

            Assert::IsTrue(manager.ReloadPlugin("com.test.shared"));
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());

            Assert::IsTrue(manager.UnloadPlugin("com.test.shared"));
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());

            std::filesystem::remove_all(pluginsRoot, error);
        }

        TEST_METHOD(LoadPlugin_IncludesExportsFromLoadedOptionalDependency)
        {
            const auto pluginsRoot = std::filesystem::temp_directory_path() / "PureMirror.OptionalPluginTests";
            std::error_code error;
            std::filesystem::remove_all(pluginsRoot, error);
            CreateOptionalExportPlugins(pluginsRoot);

            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{2}, manager.ScanPlugins(pluginsRoot));
            Assert::IsTrue(manager.LoadPlugin("com.test.optional-provider"));
            Assert::IsTrue(manager.LoadPlugin("com.test.optional-consumer"));
            manager.Render();
            Assert::AreEqual(std::size_t{2}, manager.LoadedPluginCount());

            manager.UnloadAll();
            std::filesystem::remove_all(pluginsRoot, error);
        }
    };
}  // namespace PureMirror::Overlay::Tests
