#include "pch.h"

#include "ScriptUiScopeStack.h"

namespace PureMirror::Overlay
{
    void ScriptUiScopeStack::Open(std::string ownerId,
                                  std::string openingCommand,
                                  std::string closingCommand,
                                  std::function<void()> close)
    {
        m_Scopes.push_back({.OwnerId = std::move(ownerId),
                            .OpeningCommand = std::move(openingCommand),
                            .ClosingCommand = std::move(closingCommand),
                            .Close = std::move(close)});
    }

    bool ScriptUiScopeStack::Close(const std::string_view closingCommand, const RecoveryHandler& recoveryHandler)
    {
        const auto matchingScope = std::ranges::find(m_Scopes, closingCommand, &ScriptUiScope::ClosingCommand);
        if (matchingScope == m_Scopes.end())
            return false;

        while (m_Scopes.back().ClosingCommand != closingCommand)
            RecoverTop(recoveryHandler);

        auto scope = std::move(m_Scopes.back());
        m_Scopes.pop_back();
        if (scope.Close)
            scope.Close();
        return true;
    }

    void ScriptUiScopeStack::CloseAll(const RecoveryHandler& recoveryHandler)
    {
        while (!m_Scopes.empty())
            RecoverTop(recoveryHandler);
    }

    bool ScriptUiScopeStack::IsEmpty() const noexcept
    {
        return m_Scopes.empty();
    }

    std::size_t ScriptUiScopeStack::Size() const noexcept
    {
        return m_Scopes.size();
    }

    void ScriptUiScopeStack::RecoverTop(const RecoveryHandler& recoveryHandler)
    {
        auto scope = std::move(m_Scopes.back());
        m_Scopes.pop_back();
        if (scope.Close)
            scope.Close();
        if (recoveryHandler)
            recoveryHandler(scope);
    }
}  // namespace PureMirror::Overlay
