#include "CppUnitTest.h"
#include "angelscript.h"
#include "src/scripting/angelscript/bindings/primitives/ScriptNumberBindings.h"

#include <format>
#include <memory>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        void CaptureNumberScriptMessage(const asSMessageInfo* message, void* output)
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

    TEST_CLASS(ScriptNumberTests)
    {
      public:
        TEST_METHOD(Bindings_ProvideNumericInOutWrappers)
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
            Assert::AreEqual(
                static_cast<int>(asSUCCESS),
                engine->SetMessageCallback(asFUNCTION(CaptureNumberScriptMessage), &diagnostics, asCALL_CDECL));
            Assert::IsTrue(RegisterScriptNumberBindings(*engine, error),
                           std::wstring(error.begin(), error.end()).c_str());

            auto* module = engine->GetModule("NumberTests", asGM_ALWAYS_CREATE);
            Assert::IsNotNull(module);
            constexpr char script[] = R"(
                void Increment(Int&inout value)
                {
                    value.Value = value.Value + 1;
                }

                void Increment(UInt&inout value)
                {
                    value.Value = value.Value + 1;
                }

                void Increment(Long&inout value)
                {
                    value.Value = value.Value + 1;
                }

                void Increment(ULong&inout value)
                {
                    value.Value = value.Value + 1;
                }

                bool VerifyNumbers()
                {
                    Int signedValue(-2);
                    UInt unsignedValue(2);
                    Long longValue(-3);
                    ULong unsignedLongValue(3);

                    Increment(signedValue);
                    Increment(unsignedValue);
                    Increment(longValue);
                    Increment(unsignedLongValue);

                    if (signedValue.Value != -1 || unsignedValue.Value != 3 ||
                        longValue.Value != -2 || unsignedLongValue.Value != 4)
                        return false;

                    Int@ sharedValue = Int(10);
                    Int@ alias;
                    @alias = @sharedValue;
                    Increment(alias);
                    if (sharedValue.Value != 11)
                        return false;

                    Int assignedValue;
                    assignedValue = sharedValue;
                    int convertedValue = assignedValue;
                    return convertedValue == 11;
                }
            )";
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             module->AddScriptSection("ScriptNumberTests", script, sizeof(script) - 1));
            const auto buildResult = module->Build();
            Assert::AreEqual(
                static_cast<int>(asSUCCESS), buildResult, std::wstring(diagnostics.begin(), diagnostics.end()).c_str());

            auto* function = module->GetFunctionByDecl("bool VerifyNumbers()");
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
