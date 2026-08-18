#pragma once

#include "ScriptUiScope.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class ScriptUiScopeStack
    {
      public:
        using RecoveryHandler = std::function<void(const ScriptUiScope&)>;

        void Open(std::string ownerId,
                  std::string openingCommand,
                  std::string closingCommand,
                  std::function<void()> close);
        [[nodiscard]] bool Close(std::string_view closingCommand, const RecoveryHandler& recoveryHandler);
        void CloseAll(const RecoveryHandler& recoveryHandler);

        [[nodiscard]] bool IsEmpty() const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;

      private:
        void RecoverTop(const RecoveryHandler& recoveryHandler);

        std::vector<ScriptUiScope> m_Scopes;
    };
}  // namespace PureMirror::Overlay
