#include "CppUnitTest.h"
#include "angelscript.h"
#include "src/scripting/angelscript/bindings/math/ScriptImVec2Bindings.h"

#include <format>
#include <memory>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        void CaptureImVec2ScriptMessage(const asSMessageInfo* message, void* output)
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

    TEST_CLASS(ScriptImVec2Tests)
    {
      public:
        TEST_METHOD(Bindings_ProvideConstructorsAndProperties)
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
                engine->SetMessageCallback(asFUNCTION(CaptureImVec2ScriptMessage), &diagnostics, asCALL_CDECL));
            Assert::IsTrue(RegisterScriptImVec2Bindings(*engine, error),
                           std::wstring(error.begin(), error.end()).c_str());

            auto* module = engine->GetModule("ImVec2Tests", asGM_ALWAYS_CREATE);
            Assert::IsNotNull(module);
            constexpr char script[] = R"(
                bool VerifyImVec2()
                {
                    ImVec2 defaultValue;
                    if (defaultValue.x != 0.0f || defaultValue.y != 0.0f)
                        return false;

                    ImVec2 value(12.5f, -4.0f);
                    value.x += 0.5f;
                    return value.x == 13.0f && value.y == -4.0f;
                }
            )";
            Assert::AreEqual(static_cast<int>(asSUCCESS),
                             module->AddScriptSection("ScriptImVec2Tests", script, sizeof(script) - 1));
            const auto buildResult = module->Build();
            Assert::AreEqual(
                static_cast<int>(asSUCCESS), buildResult, std::wstring(diagnostics.begin(), diagnostics.end()).c_str());

            auto* function = module->GetFunctionByDecl("bool VerifyImVec2()");
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
