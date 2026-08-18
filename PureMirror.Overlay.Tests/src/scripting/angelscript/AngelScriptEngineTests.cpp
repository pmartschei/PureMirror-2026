#include "CppUnitTest.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
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

            std::size_t BeginCallCount{};
            std::size_t EndCallCount{};
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
            const auto callback = engine.CallFunction("com.example.consumer", "void on_load()");

            Assert::IsTrue(bindings.IsSuccessful());
            Assert::IsTrue(callback.IsSuccessful());
        }

        TEST_METHOD(CallFunction_TreatsMissingCallbackAsSuccessfulAndReportsExceptions)
        {
            AngelScriptEngine engine;
            const std::vector sources{
                ScriptSource{"main.as", "int divide(int value) { return 1 / value; } void explode() { divide(0); }"}};
            Assert::IsTrue(engine.LoadModule("com.example.calls", sources).IsSuccessful());

            const auto missing = engine.CallFunction("com.example.calls", "void optional_callback()");
            const auto failed = engine.CallFunction("com.example.calls", "void explode()");

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
            const auto result = engine.CallFunction("com.example.timeout", "void run()");
            const auto duration = std::chrono::steady_clock::now() - startedAt;

            Assert::IsFalse(result.IsSuccessful());
            Assert::IsTrue(duration < std::chrono::seconds(1));
            Assert::AreEqual(std::size_t{1}, host.BeginCallCount);
            Assert::AreEqual(std::size_t{1}, host.EndCallCount);
            Assert::IsFalse(result.Diagnostics.empty());
            Assert::IsTrue(result.Diagnostics.front().Message.find("100 ms") != std::string::npos);
        }
    };
}  // namespace PureMirror::Overlay::Tests
