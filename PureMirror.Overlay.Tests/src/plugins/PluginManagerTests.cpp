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
            void BeginScriptCall(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
            }

            void EndScriptCall(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
            }

            void LogInfo(std::string_view pluginId, std::string_view message) override
            {
                static_cast<void>(pluginId);
                LogMessages.emplace_back(message);
            }

            bool BeginWindow(std::string_view pluginId,
                             std::string_view title,
                             bool* open,
                             std::uint32_t flags) override
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

            bool Button(std::string_view pluginId, std::string_view label, float width, float height) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(label);
                return false;
            }

            bool BeginMenu(std::string_view pluginId, std::string_view label, bool enabled) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(label);
                return true;
            }

            void EndMenu(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
            }

            bool MenuItem(std::string_view pluginId,
                          std::string_view label,
                          std::string_view shortcut,
                          bool selected,
                          bool enabled) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(label);
                return false;
            }

            void MenuSeparator(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
            }

            std::vector<std::string> LogMessages;
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
            std::ofstream(pluginRoot / "scripts/main.as") << "void OnLoad() {}\n"
                                                             "void OnRenderInterface() {}\n"
                                                             "void OnUnload() {}\n";
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
                                                               "void OnLoad() {}\n"
                                                               "void OnRenderInterface() {}\n"
                                                               "void OnUnload() {}\n";
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
            std::ofstream(consumerRoot / "scripts/main.as") << "void OnLoad() {}\n"
                                                               "void OnRenderInterface() { optional_value(); }\n"
                                                               "void OnUnload() {}\n";
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

            Assert::AreEqual(std::size_t{7}, discoveredCount);
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());
            Assert::AreEqual(std::size_t{7}, manager.AvailablePlugins().size());

            Assert::IsTrue(manager.LoadPlugin("com.puremirror.example.hello-overlay"));
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());
            manager.Render();
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());

            Assert::IsTrue(manager.UnloadPlugin("com.puremirror.example.hello-overlay"));
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());
        }

        TEST_METHOD(FrameCallbacks_ExecuteInOrderAndReceiveDeltaTime)
        {
            const auto pluginsRoot = std::filesystem::temp_directory_path() / "PureMirror.PluginCallbackTests";
            std::error_code error;
            std::filesystem::remove_all(pluginsRoot, error);
            CreatePlugin(pluginsRoot, "com.test.callbacks", "Callbacks");
            std::ofstream(pluginsRoot / "com.test.callbacks/scripts/main.as") << R"(
void OnLoad() { log::info("load"); }
void OnBeginFrame() { log::info("begin"); }
void OnUpdate(float deltaTime) { if (deltaTime > 0.24f && deltaTime < 0.26f) log::info("update"); }
void OnRenderMenu() { UI::MenuItem("Callback entry"); log::info("menu"); }
void OnRenderInterface() { UI::Text("interface"); log::info("interface"); }
void OnEndFrame() { log::info("end"); }
void OnUnload() { log::info("unload"); }
)";

            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{1}, manager.ScanPlugins(pluginsRoot));
            Assert::IsTrue(manager.LoadPlugin("com.test.callbacks"));
            manager.BeginFrame();
            manager.Update(0.25F);
            manager.RenderMenu();
            manager.RenderInterface();
            manager.EndFrame();
            manager.UnloadAll();

            const std::vector<std::string> expected{"load", "begin", "update", "menu", "interface", "end", "unload"};
            Assert::AreEqual(expected.size(), scriptHost.LogMessages.size());
            for (std::size_t index{}; index < expected.size(); ++index)
                Assert::AreEqual(expected[index], scriptHost.LogMessages[index]);

            std::filesystem::remove_all(pluginsRoot, error);
        }
        TEST_METHOD(LegacyCallbacks_AreNotInvoked)
        {
            const auto pluginsRoot = std::filesystem::temp_directory_path() / "PureMirror.LegacyCallbackTests";
            std::error_code error;
            std::filesystem::remove_all(pluginsRoot, error);
            CreatePlugin(pluginsRoot, "com.test.legacy-callbacks", "Legacy Callbacks");
            std::ofstream(pluginsRoot / "com.test.legacy-callbacks/scripts/main.as") << R"(
void on_load() { log::info("legacy-load"); }
void on_render() { log::info("legacy-render"); }
void on_unload() { log::info("legacy-unload"); }
)";

            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{1}, manager.ScanPlugins(pluginsRoot));
            Assert::IsTrue(manager.LoadPlugin("com.test.legacy-callbacks"));
            manager.Render();
            manager.UnloadAll();

            Assert::IsTrue(scriptHost.LogMessages.empty());
            std::filesystem::remove_all(pluginsRoot, error);
        }
        TEST_METHOD(ExamplePlugins_LoadDependencyAndCyclicGroups)
        {
            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{7}, manager.ScanPlugins(ExamplePluginsRoot()));

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

        TEST_METHOD(RenderFailure_LogsSubsequentUnloadFailure)
        {
            const auto pluginsRoot = std::filesystem::temp_directory_path() / "PureMirror.PluginUnloadTimeoutTests";
            std::error_code error;
            std::filesystem::remove_all(pluginsRoot, error);
            CreatePlugin(pluginsRoot, "com.test.timeout", "Timeout");
            std::ofstream(pluginsRoot / "com.test.timeout/scripts/main.as")
                << "void OnLoad() {}\n"
                   "void OnRenderInterface() { while (true) {} }\n"
                   "void OnUnload() { while (true) {} }\n";

            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            Assert::AreEqual(std::size_t{1}, manager.ScanPlugins(pluginsRoot));
            Assert::IsTrue(manager.LoadPlugin("com.test.timeout"));
            manager.Render();

            const auto messages = logger.Snapshot();
            const auto containsMessageId = [&](const std::string_view messageId)
            { return std::ranges::find(messages, messageId, &LogMessage::MessageId) != messages.end(); };
            Assert::IsTrue(containsMessageId("plugins.script.render-interface.com.test.timeout"));
            Assert::IsTrue(containsMessageId("plugins.script.unload.com.test.timeout"));
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());

            std::filesystem::remove_all(pluginsRoot, error);
        }
    };
}  // namespace PureMirror::Overlay::Tests
