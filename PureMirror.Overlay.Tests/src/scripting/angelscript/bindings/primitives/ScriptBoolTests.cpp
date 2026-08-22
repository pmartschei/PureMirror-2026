#include "CppUnitTest.h"
#include "angelscript.h"
#include "src/scripting/angelscript/bindings/primitives/ScriptBoolBindings.h"

#include <format>
#include <memory>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        void CaptureScriptMessage(const asSMessageInfo* message, void* output)
        {
            if (message == nullptr || output == nullptr)
                return;
            *static_cast<std::string*>(output) += std::format("{}({},{}) [{}]: {}\n",
                                                              message->section,
                                                              message->row,
                                                              message->col,
                                                              static_cast<int>(message->type),
                                                              message->message);
        }
    }  // namespace

    TEST_CLASS(ScriptBoolTests)
    {
      public:
        TEST_METHOD(Bindings_ProvideFactoriesAssignmentsConversionAndValueProperty)
        {
            const auto engine =
                std::unique_ptr<asIScriptEngine, void (*)(asIScriptEngine*)>(asCreateScriptEngine(),
                                                                             [](asIScriptEngine* value)
                                                                             {
                                                                                 if (value != nullptr)
                                                                                     value->ShutDownAndRelease();
                                                                             });
            Assert::IsNotNull(engine.get());

            std::string error;
            std::string diagnostics;
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             engine->SetMessageCallback(asFUNCTION(CaptureScriptMessage), &diagnostics, asCALL_CDECL));
            Assert::IsTrue(RegisterScriptBoolBindings(*engine, error),
                           std::wstring(error.begin(), error.end()).c_str());

            auto* module = engine->GetModule("BoolTests", asGM_ALWAYS_CREATE);
            Assert::IsNotNull(module);
            constexpr char script[] = R"(
                bool VerifyBool()
                {
                    Bool defaultValue;
                    Bool trueValue(true);
                    if (defaultValue.Value || !trueValue.Value)
                        return false;

                    defaultValue.Value = true;
                    bool defaultConvertedValue = defaultValue;
                    if (!defaultConvertedValue)
                        return false;

                    Bool assignedValue;
                    assignedValue = trueValue;
                    if (!assignedValue.Value)
                        return false;

                    assignedValue = false;
                    bool convertedValue = assignedValue;
                    return !convertedValue;
                }
            )";
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             module->AddScriptSection("ScriptBoolTests", script, sizeof(script) - 1));
            const auto buildResult = module->Build();
            Assert::AreEqual(
                static_cast<int>(asSUCCESS), buildResult, std::wstring(diagnostics.begin(), diagnostics.end()).c_str());

            auto* function = module->GetFunctionByDecl("bool VerifyBool()");
            Assert::IsNotNull(function);
            const auto context = std::unique_ptr<asIScriptContext, void (*)(asIScriptContext*)>(
                engine->CreateContext(), [](asIScriptContext* value) { value->Release(); });
            Assert::IsNotNull(context.get());
            Assert::AreEqual(static_cast<int>(asSUCCESS), context->Prepare(function));
            Assert::AreEqual(static_cast<int>(asEXECUTION_FINISHED), context->Execute());
            Assert::AreNotEqual(asBYTE{}, context->GetReturnByte());
        }

        TEST_METHOD(Bindings_ProvideReferenceCountedHandles)
        {
            const auto engine =
                std::unique_ptr<asIScriptEngine, void (*)(asIScriptEngine*)>(asCreateScriptEngine(),
                                                                             [](asIScriptEngine* value)
                                                                             {
                                                                                 if (value != nullptr)
                                                                                     value->ShutDownAndRelease();
                                                                             });
            Assert::IsNotNull(engine.get());

            std::string error;
            std::string diagnostics;
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             engine->SetMessageCallback(asFUNCTION(CaptureScriptMessage), &diagnostics, asCALL_CDECL));
            Assert::IsTrue(RegisterScriptBoolBindings(*engine, error),
                           std::wstring(error.begin(), error.end()).c_str());

            auto* module = engine->GetModule("BoolHandleTests", asGM_ALWAYS_CREATE);
            Assert::IsNotNull(module);
            constexpr char script[] = R"(
                bool VerifyBoolHandles()
                {
                    Bool@ empty;
                    if (empty !is null)
                        return false;

                    Bool@ first = Bool(true);
                    Bool@ second;
                    @second = @first;
                    if (second !is first)
                        return false;

                    second.Value = false;
                    if (first.Value)
                        return false;

                    @second = null;
                    if (second !is null || first is null)
                        return false;

                    Bool@ surviving;
                    {
                        Bool@ temporary = Bool(true);
                        @surviving = @temporary;
                    }

                    return surviving !is null && surviving.Value;
                }
            )";
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             module->AddScriptSection("ScriptBoolHandleTests", script, sizeof(script) - 1));
            const auto buildResult = module->Build();
            Assert::AreEqual(
                static_cast<int>(asSUCCESS), buildResult, std::wstring(diagnostics.begin(), diagnostics.end()).c_str());

            auto* function = module->GetFunctionByDecl("bool VerifyBoolHandles()");
            Assert::IsNotNull(function);
            const auto context = std::unique_ptr<asIScriptContext, void (*)(asIScriptContext*)>(
                engine->CreateContext(), [](asIScriptContext* value) { value->Release(); });
            Assert::IsNotNull(context.get());
            Assert::AreEqual(static_cast<int>(asSUCCESS), context->Prepare(function));
            Assert::AreEqual(static_cast<int>(asEXECUTION_FINISHED), context->Execute());
            Assert::AreNotEqual(asBYTE{}, context->GetReturnByte());
        }
    };
}  // namespace PureMirror::Overlay::Tests
