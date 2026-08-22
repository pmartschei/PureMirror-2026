#include "CppUnitTest.h"
#include "src/plugins/PluginManifestParser.h"
#include "src/scripting/IScriptHost.h"
#include "src/scripting/PluginScriptInstance.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        class RecordingScriptHost final : public IScriptHost
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

            void LogInfo(const std::string_view pluginId, const std::string_view message) override
            {
                PluginIds.emplace_back(pluginId);
                LogMessages.emplace_back(message);
            }

            bool BeginWindow(std::string_view pluginId,
                             std::string_view title,
                             bool* open,
                             std::uint32_t flags) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(title);
                ++BeginWindowCallCount;
                return true;
            }

            void EndWindow(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
                ++EndWindowCallCount;
            }

            void Text(std::string_view pluginId, const std::string_view value) override
            {
                static_cast<void>(pluginId);
                TextValues.emplace_back(value);
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

            std::vector<std::string> PluginIds;
            std::vector<std::string> LogMessages;
            std::vector<std::string> TextValues;
            std::size_t BeginWindowCallCount{};
            std::size_t EndWindowCallCount{};
        };

        std::filesystem::path ExamplePluginRoot(const std::string_view pluginDirectory = "hello-overlay")
        {
            auto repositoryRoot = std::filesystem::path(__FILE__).parent_path();
            for (int level{}; level < 3; ++level)
                repositoryRoot = repositoryRoot.parent_path();
            return repositoryRoot / "PureMirror.Overlay/examples/plugins" / pluginDirectory;
        }
    }  // namespace

    TEST_CLASS(ExamplePluginIntegrationTests)
    {
      public:
        TEST_METHOD(LoadRenderAndUnload_ExecutesHelloOverlayExample)
        {
            const auto pluginRoot = ExamplePluginRoot();
            std::ifstream manifestStream(pluginRoot / "plugin.json", std::ios::binary);
            const std::string json{std::istreambuf_iterator<char>{manifestStream}, std::istreambuf_iterator<char>{}};
            const auto parsedManifest = PluginManifestParser{}.Parse(json);
            Assert::IsTrue(parsedManifest.IsSuccessful());

            RecordingScriptHost host;
            AngelScriptEngine engine(&host);
            PluginScriptInstance plugin(engine, parsedManifest.Manifest, pluginRoot);

            const auto loaded = plugin.Load();
            const auto rendered = plugin.Render();
            const auto unloaded = plugin.Unload();

            Assert::IsTrue(loaded.IsSuccessful());
            Assert::IsTrue(rendered.IsSuccessful());
            Assert::IsTrue(unloaded.IsSuccessful());
            Assert::AreEqual(std::size_t{2}, host.LogMessages.size());
            Assert::AreEqual(std::string{"Hello Overlay loaded"}, host.LogMessages.front());
            Assert::AreEqual(std::string{"Hello Overlay unloaded"}, host.LogMessages.back());
            Assert::AreEqual(std::string{"com.puremirror.example.hello-overlay"}, host.PluginIds.front());
            Assert::AreEqual(std::size_t{2}, host.TextValues.size());
        }

        TEST_METHOD(Compile_AcceptsAsyncTasksExample)
        {
            const auto pluginRoot = ExamplePluginRoot("async-tasks");
            std::ifstream manifestStream(pluginRoot / "plugin.json", std::ios::binary);
            const std::string json{std::istreambuf_iterator<char>{manifestStream}, std::istreambuf_iterator<char>{}};
            const auto parsedManifest = PluginManifestParser{}.Parse(json);
            Assert::IsTrue(parsedManifest.IsSuccessful());

            RecordingScriptHost host;
            AngelScriptEngine engine(&host);
            PluginScriptInstance plugin(engine, parsedManifest.Manifest, pluginRoot);

            const auto compiled = plugin.Compile();

            Assert::IsTrue(compiled.IsSuccessful());
            Assert::IsTrue(plugin.Unload().IsSuccessful());
        }

        TEST_METHOD(Render_IntentionalTimeoutExampleExceedsDeadlineBeforeClosingWindow)
        {
            const auto pluginRoot = ExamplePluginRoot("render-timeout");
            std::ifstream manifestStream(pluginRoot / "plugin.json", std::ios::binary);
            const std::string json{std::istreambuf_iterator<char>{manifestStream}, std::istreambuf_iterator<char>{}};
            const auto parsedManifest = PluginManifestParser{}.Parse(json);
            Assert::IsTrue(parsedManifest.IsSuccessful());

            RecordingScriptHost host;
            AngelScriptEngine engine(&host);
            PluginScriptInstance plugin(engine, parsedManifest.Manifest, pluginRoot);

            Assert::IsTrue(plugin.Load().IsSuccessful());
            const auto rendered = plugin.Render();

            Assert::IsFalse(rendered.IsSuccessful());
            Assert::IsFalse(rendered.Diagnostics.empty());
            Assert::IsTrue(rendered.Diagnostics.front().Message.find("100 ms") != std::string::npos);
            Assert::AreEqual(std::size_t{1}, host.BeginWindowCallCount);
            Assert::AreEqual(std::size_t{0}, host.EndWindowCallCount);
            Assert::IsTrue(plugin.Unload().IsSuccessful());
        }
    };
}  // namespace PureMirror::Overlay::Tests
