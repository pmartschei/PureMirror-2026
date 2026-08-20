#pragma once

#include "angelscript.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace PureMirror::Overlay
{
    class ScriptTask
    {
      public:
        ScriptTask(asIScriptEngine& engine, int resultTypeId);

        void AddRef() const noexcept;
        void Release() const noexcept;

        void Complete(asIScriptContext& context, std::uint64_t completionOrder);
        void Fail(std::string message, std::uint64_t completionOrder);
        void ResolveTo(ScriptTask& task);

        [[nodiscard]] bool IsCompleted() const noexcept;
        [[nodiscard]] bool IsFailed() const noexcept;
        [[nodiscard]] std::uint64_t CompletionOrder() const noexcept;
        [[nodiscard]] int ResultTypeId() const noexcept;
        [[nodiscard]] std::string_view Error() const noexcept;
        [[nodiscard]] bool Retrieve(void* output, int typeId) const;
        [[nodiscard]] bool Retrieve(asINT64& output) const noexcept;
        [[nodiscard]] bool Retrieve(double& output) const noexcept;
        [[nodiscard]] bool Retrieve(asIScriptGeneric& generic) const;
        [[nodiscard]] bool Cast(void* output, int typeId) const;

        [[nodiscard]] static int NormalizeTypeId(int typeId) noexcept;

      private:
        ~ScriptTask();

        void ReleaseResult() noexcept;
        [[nodiscard]] const ScriptTask& ResolvedTask() const noexcept;

        asIScriptEngine& m_Engine;
        mutable std::atomic_int m_ReferenceCount{1};
        int m_ResultTypeId{};
        asINT64 m_IntegerResult{};
        double m_DoubleResult{};
        void* m_ObjectResult{};
        asITypeInfo* m_ObjectType{};
        ScriptTask* m_ResolvedTask{};
        std::string m_Error;
        std::uint64_t m_CompletionOrder{};
        bool m_IsCompleted{};
        bool m_IsFailed{};
    };
}  // namespace PureMirror::Overlay
