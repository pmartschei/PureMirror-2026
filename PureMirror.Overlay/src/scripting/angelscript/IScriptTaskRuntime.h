#pragma once

class asIScriptGeneric;
class CScriptArray;

namespace PureMirror::Overlay
{
    class ScriptTask;

    class IScriptTaskRuntime
    {
      public:
        virtual ~IScriptTaskRuntime() = default;

        virtual void HostAsync(asIScriptGeneric& generic) = 0;
        virtual void HostWait(ScriptTask* task) = 0;
        virtual void HostWaitAll(CScriptArray* tasks) = 0;
        virtual void HostWaitAny(asIScriptGeneric& generic) = 0;
    };
}  // namespace PureMirror::Overlay
