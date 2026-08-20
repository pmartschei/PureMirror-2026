#include "CppUnitTest.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        class ExecutionTrackingScriptHost final : public IScriptHost
        {
          public:
            void BeginScriptCall(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
                ++BeginCallCount;
            }

            void EndScriptCall(std::string_view pluginId) override
            {
                static_cast<void>(pluginId);
                ++EndCallCount;
            }

            void LogInfo(std::string_view pluginId, std::string_view message) override
            {
                static_cast<void>(pluginId);
                LogMessages.emplace_back(message);
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
                ++TextCallCount;
            }

            bool Button(std::string_view pluginId, std::string_view label) override
            {
                static_cast<void>(pluginId);
                static_cast<void>(label);
                return false;
            }

            std::size_t BeginCallCount{};
            std::size_t EndCallCount{};
            std::size_t TextCallCount{};
            std::vector<std::string> LogMessages;
        };
    }  // namespace

    TEST_CLASS(AngelScriptEngineTests)
    {
      public:
        TEST_METHOD(LoadModule_CompilesMultipleSectionsAndStandardStrings)
        {
            AngelScriptEngine engine;
            const std::vector sources{ScriptSource{"main.as", R"(
                string greeting = "Hello";
                void on_load() { exported_helper(); }
            )"},
                                      ScriptSource{"exports/helper.as", "void exported_helper() {}"}};

            const auto result = engine.LoadModule("com.example.valid", sources);

            Assert::IsTrue(engine.IsInitialized());
            Assert::IsTrue(result.IsSuccessful());
            Assert::AreEqual(std::size_t{0}, result.Diagnostics.size());
        }

        TEST_METHOD(LoadModule_ReturnsCompilerDiagnosticsWithSectionAndPosition)
        {
            AngelScriptEngine engine;
            const std::vector sources{ScriptSource{"broken.as", "void on_load( {"}};

            const auto result = engine.LoadModule("com.example.broken", sources);

            Assert::IsFalse(result.IsSuccessful());
            Assert::IsFalse(result.Diagnostics.empty());
            Assert::AreEqual(std::string{"broken.as"}, result.Diagnostics.back().Section);
            Assert::IsTrue(result.Diagnostics.back().Row > 0);
        }

        TEST_METHOD(UnloadModule_AllowsModuleToBeCompiledAgain)
        {
            AngelScriptEngine engine;
            const std::vector sources{ScriptSource{"main.as", "void on_load() {}"}};
            Assert::IsTrue(engine.LoadModule("com.example.reload", sources).IsSuccessful());

            engine.UnloadModule("com.example.reload");
            const auto result = engine.LoadModule("com.example.reload", sources);

            Assert::IsTrue(result.IsSuccessful());
        }

        TEST_METHOD(BindModuleImports_BindsFunctionsFromLoadedProvider)
        {
            AngelScriptEngine engine;
            const std::vector providerSources{ScriptSource{"main.as", "int exported_value() { return 42; }"}};
            const std::vector consumerSources{ScriptSource{"main.as",
                                                           "import int exported_value() from \"com.example.provider\"; "
                                                           "void on_load() { exported_value(); }"}};

            Assert::IsTrue(engine.LoadModule("com.example.provider", providerSources).IsSuccessful());
            Assert::IsTrue(engine.LoadModule("com.example.consumer", consumerSources).IsSuccessful());

            const auto bindings = engine.BindModuleImports("com.example.consumer");
            const auto callback = engine.CallFunction("com.example.consumer", {"void on_load()"});

            Assert::IsTrue(bindings.IsSuccessful());
            Assert::IsTrue(callback.IsSuccessful());
        }

        TEST_METHOD(CallFunction_TreatsMissingCallbackAsSuccessfulAndReportsExceptions)
        {
            AngelScriptEngine engine;
            const std::vector sources{
                ScriptSource{"main.as", "int divide(int value) { return 1 / value; } void explode() { divide(0); }"}};
            Assert::IsTrue(engine.LoadModule("com.example.calls", sources).IsSuccessful());

            const auto missing = engine.CallFunction("com.example.calls", {"void optional_callback()"});
            const auto failed = engine.CallFunction("com.example.calls", {"void explode()"});

            Assert::IsTrue(missing.IsSuccessful());
            Assert::IsTrue(missing.Status == ScriptCallStatus::NotFound);
            Assert::IsFalse(failed.IsSuccessful());
            Assert::IsFalse(failed.Diagnostics.empty());
        }

        TEST_METHOD(CallFunction_AbortsAfterOneHundredMillisecondsAndFinishesHostCall)
        {
            ExecutionTrackingScriptHost host;
            AngelScriptEngine engine(&host);
            const std::vector sources{ScriptSource{"main.as", "void run() { while (true) {} }"}};
            Assert::IsTrue(engine.LoadModule("com.example.timeout", sources).IsSuccessful());

            const auto startedAt = std::chrono::steady_clock::now();
            const auto result = engine.CallFunction("com.example.timeout", {"void run()"});
            const auto duration = std::chrono::steady_clock::now() - startedAt;

            Assert::IsFalse(result.IsSuccessful());
            Assert::IsTrue(duration < std::chrono::seconds(1));
            Assert::AreEqual(std::size_t{1}, host.BeginCallCount);
            Assert::AreEqual(std::size_t{1}, host.EndCallCount);
            Assert::IsFalse(result.Diagnostics.empty());
            Assert::IsTrue(result.Diagnostics.front().Message.find("100 ms") != std::string::npos);
        }

        TEST_METHOD(Yield_ResumesSuspendableCallbackOnTheNextFrame)
        {
            ExecutionTrackingScriptHost host;
            AngelScriptEngine engine(&host);
            const std::vector sources{
                ScriptSource{"main.as", "void run() { log::info(\"before\"); Utils::Yield(); log::info(\"after\"); }"}};
            Assert::IsTrue(engine.LoadModule("com.example.yield", sources).IsSuccessful());
            constexpr ScriptCallback callback{"void run()", ScriptCallbackTag::Suspendable};

            const auto suspended = engine.CallFunction("com.example.yield", callback);
            const auto sameFrame = engine.CallFunction("com.example.yield", callback);
            engine.AdvanceFrame();
            const auto resumed = engine.CallFunction("com.example.yield", callback);

            Assert::IsTrue(suspended.Status == ScriptCallStatus::Suspended);
            Assert::IsTrue(sameFrame.Status == ScriptCallStatus::Suspended);
            Assert::IsTrue(resumed.Status == ScriptCallStatus::Executed);
            Assert::AreEqual(std::size_t{2}, host.BeginCallCount);
            Assert::AreEqual(std::size_t{2}, host.EndCallCount);
            Assert::AreEqual(std::size_t{2}, host.LogMessages.size());
            Assert::AreEqual(std::string{"before"}, host.LogMessages.front());
            Assert::AreEqual(std::string{"after"}, host.LogMessages.back());
        }

        TEST_METHOD(Sleep_ResumesAfterItsDelayAndAFrameBoundary)
        {
            AngelScriptEngine engine;
            const std::vector sources{ScriptSource{"main.as", "void run() { Utils::Sleep(0); }"}};
            Assert::IsTrue(engine.LoadModule("com.example.sleep", sources).IsSuccessful());
            constexpr ScriptCallback callback{"void run()", ScriptCallbackTag::Suspendable};

            const auto suspended = engine.CallFunction("com.example.sleep", callback);
            engine.AdvanceFrame();
            const auto resumed = engine.CallFunction("com.example.sleep", callback);

            Assert::IsTrue(suspended.Status == ScriptCallStatus::Suspended);
            Assert::IsTrue(resumed.Status == ScriptCallStatus::Executed);
        }

        TEST_METHOD(Sleep_SuspendsAndResumesMultiplePluginModulesIndependently)
        {
            ExecutionTrackingScriptHost host;
            AngelScriptEngine engine(&host);
            constexpr std::array<std::string_view, 4> moduleIds{
                "com.example.sleep.one", "com.example.sleep.two", "com.example.sleep.three", "com.example.sleep.four"};
            const std::vector sources{ScriptSource{
                "main.as", "void on_load() { log::info(\"before\"); Utils::Sleep(50); log::info(\"after\"); }"}};
            constexpr ScriptCallback callback{"void on_load()", ScriptCallbackTag::Suspendable};

            for (const auto moduleId : moduleIds)
                Assert::IsTrue(engine.LoadModule(moduleId, sources).IsSuccessful());

            for (const auto moduleId : moduleIds)
            {
                const auto result = engine.CallFunction(moduleId, callback);
                Assert::IsTrue(result.Status == ScriptCallStatus::Suspended);
            }

            Assert::AreEqual(moduleIds.size(), host.LogMessages.size());
            Assert::AreEqual(moduleIds.size(), host.BeginCallCount);
            Assert::AreEqual(moduleIds.size(), host.EndCallCount);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            engine.AdvanceFrame();
            for (const auto moduleId : moduleIds)
            {
                const auto result = engine.CallFunction(moduleId, callback);
                Assert::IsTrue(result.Status == ScriptCallStatus::Executed);
            }

            Assert::AreEqual(moduleIds.size() * 2, host.LogMessages.size());
            Assert::AreEqual(moduleIds.size() * 2, host.BeginCallCount);
            Assert::AreEqual(moduleIds.size() * 2, host.EndCallCount);
        }

        TEST_METHOD(CallbackTags_RejectUnavailableSuspensionAndUiCapabilities)
        {
            ExecutionTrackingScriptHost host;
            AngelScriptEngine engine(&host);
            const std::vector sources{
                ScriptSource{"main.as", "void render() { Utils::Yield(); } void load() { ui::text(\"no\"); }"}};
            Assert::IsTrue(engine.LoadModule("com.example.capabilities", sources).IsSuccessful());

            const auto render =
                engine.CallFunction("com.example.capabilities", {"void render()", ScriptCallbackTag::Ui});
            const auto load =
                engine.CallFunction("com.example.capabilities", {"void load()", ScriptCallbackTag::Suspendable});

            Assert::IsFalse(render.IsSuccessful());
            Assert::IsFalse(load.IsSuccessful());
            Assert::AreEqual(std::size_t{0}, host.TextCallCount);
            Assert::IsTrue(render.Diagnostics.front().Message.find("Yield") != std::string::npos);
            Assert::IsTrue(load.Diagnostics.front().Message.find("UI functions") != std::string::npos);
        }
    };
}  // namespace PureMirror::Overlay::Tests
