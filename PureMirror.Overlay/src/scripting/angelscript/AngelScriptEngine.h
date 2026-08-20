#pragma once

#include "src/scripting/IScriptEngine.h"
#include "src/scripting/IScriptHost.h"

#include <memory>

namespace PureMirror::Overlay
{
    class AngelScriptEngine final : public IScriptEngine
    {
      public:
        explicit AngelScriptEngine(IScriptHost* scriptHost = nullptr);
        ~AngelScriptEngine() override;

        AngelScriptEngine(const AngelScriptEngine&) = delete;
        AngelScriptEngine& operator=(const AngelScriptEngine&) = delete;
        AngelScriptEngine(AngelScriptEngine&&) noexcept;
        AngelScriptEngine& operator=(AngelScriptEngine&&) noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] ScriptModuleLoadResult LoadModule(std::string_view moduleId,
                                                        const std::vector<ScriptSource>& sources) override;
        [[nodiscard]] ScriptModuleLoadResult BindModuleImports(std::string_view moduleId) override;
        void AdvanceFrame() override;
        [[nodiscard]] ScriptCallResult CallFunction(std::string_view moduleId, const ScriptCallback& callback) override;
        void UnloadModule(std::string_view moduleId) override;

      private:
        class Implementation;
        std::unique_ptr<Implementation> m_Implementation;
    };
}  // namespace PureMirror::Overlay
