#pragma once

#include <string>
#include <string_view>

namespace PureMirror::Overlay
{
    class ScriptBindingUtils
    {
      public:
        explicit ScriptBindingUtils(std::string bindingGroup);

        [[nodiscard]] bool Require(int code, std::string_view operation, std::string& error) const;
        [[nodiscard]] bool operator()(int code, std::string_view operation, std::string& error) const;

      private:
        std::string m_BindingGroup;
    };
}  // namespace PureMirror::Overlay
