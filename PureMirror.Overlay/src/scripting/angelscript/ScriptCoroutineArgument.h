#pragma once

#include "angelscript.h"

#include <array>
#include <cstddef>

namespace PureMirror::Overlay
{
    class ScriptCoroutineArgument
    {
      public:
        ScriptCoroutineArgument(asIScriptEngine& engine, void* value, int typeId);
        ~ScriptCoroutineArgument();

        ScriptCoroutineArgument(const ScriptCoroutineArgument&) = delete;
        ScriptCoroutineArgument& operator=(const ScriptCoroutineArgument&) = delete;

        [[nodiscard]] void* Value() noexcept;

      private:
        asIScriptEngine& m_Engine;
        std::array<std::byte, sizeof(asQWORD)> m_PrimitiveValue{};
        void* m_ObjectValue{};
        asITypeInfo* m_ObjectType{};
        bool m_IsObject{};
    };
}  // namespace PureMirror::Overlay
