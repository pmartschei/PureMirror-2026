#include "CppUnitTest.h"
#include "src/core/logger/Logger.h"
#include "src/plugins/PluginManager.h"
#include "src/scripting/IScriptHost.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <filesystem>
#include <string_view>

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
    }  // namespace

    TEST_CLASS(PluginManagerTests)
    {
      public:
        TEST_METHOD(LoadStartupPlugins_LoadsRendersAndUnloadsExamplePlugin)
        {
            Logger logger;
            PluginManagerScriptHost scriptHost;
            AngelScriptEngine scriptEngine(&scriptHost);
            PluginManager manager(scriptEngine, logger);

            const auto loadedCount = manager.LoadStartupPlugins(ExamplePluginsRoot());

            Assert::AreEqual(std::size_t{1}, loadedCount);
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());
            manager.Render();
            Assert::AreEqual(std::size_t{1}, manager.LoadedPluginCount());

            manager.UnloadAll();
            Assert::AreEqual(std::size_t{0}, manager.LoadedPluginCount());
        }
    };
}  // namespace PureMirror::Overlay::Tests
