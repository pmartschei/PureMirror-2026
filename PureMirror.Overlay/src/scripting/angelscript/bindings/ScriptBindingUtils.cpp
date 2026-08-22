#include "pch.h"

#include "scripting/angelscript/bindings/ScriptBindingUtils.h"

#include <format>

namespace PureMirror::Overlay
{
    ScriptBindingUtils::ScriptBindingUtils(std::string bindingGroup) : m_BindingGroup(std::move(bindingGroup)) {}

    bool ScriptBindingUtils::Require(const int code, const std::string_view operation, std::string& error) const
    {
        if (code >= 0)
            return true;
        error = std::format("AngelScript {} binding failed at {} with code {}.", m_BindingGroup, operation, code);
        return false;
    }

    bool ScriptBindingUtils::operator()(const int code, const std::string_view operation, std::string& error) const
    {
        return Require(code, operation, error);
    }
}  // namespace PureMirror::Overlay
