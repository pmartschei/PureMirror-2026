# AngelScript Adapter

Der Adapter verwendet AngelScript 2.38.0 als statisch gelinkte Bibliothek. Er besitzt die Engine, registriert den offiziellen `std::string`-Typ, kompiliert mehrere Script-Sections zu einem Modul, sammelt strukturierte Diagnostik und kann Module wieder entladen.

AngelScript-Header und -Typen bleiben durch ein Pimpl vollständig in diesem Ordner. Der engine-unabhängige Layer kennt ausschließlich `IScriptEngine`, `ScriptSource` und `ScriptModuleLoadResult`.

Implementiert sind folgende optionale Lifecycle-Callbacks:

| Callback | Zeitpunkt | Capabilities |
| --- | --- | --- |
| `void OnLoad()` | nach dem Laden | suspendierbar, Coroutinen, keine UI |
| `void OnUnload()` | vor dem Entladen | keine Suspension, Coroutinen oder UI |
| `void OnBeginFrame()` | am Frame-Anfang | Coroutinen, keine Suspension oder UI |
| `void OnEndFrame()` | am Frame-Ende | Coroutinen, keine Suspension oder UI |
| `void OnUpdate(float deltaTime)` | vor `OnRenderInterface`; Sekunden seit dem vorherigen Frame | Coroutinen, keine Suspension oder UI |
| `void OnDisable()` | beim späteren Disable-Support | Coroutinen, keine Suspension oder UI |
| `void OnEnable()` | beim späteren Enable-Support | Coroutinen, keine Suspension oder UI |
| `void OnRenderMenu()` | innerhalb der Host-Menüleiste | Coroutinen, keine Suspension, ausschließlich Menu-UI |
| `void OnRenderSettings()` | beim späteren Plugin-Settings-Support | Coroutinen, keine Suspension, normale UI |
| `void OnRenderInterface()` | beim Rendern der Plugin-Oberfläche | Coroutinen, keine Suspension, normale UI |

`OnDisable`/`OnEnable` und `OnRenderSettings` sind als Callback-Verträge vorhanden, werden aber erst aufgerufen, sobald die zugehörigen Host-Funktionen implementiert sind.

Menu-Plugins verwenden in `OnRenderMenu` die Bindings `ui::begin_menu`, `ui::end_menu`, `ui::menu_item` und `ui::menu_separator`. Normale UI-Bindings sind dort absichtlich gesperrt; die Menu-Bindings sind außerhalb dieses Callbacks gesperrt.

Callbacks tragen engine-unabhängige Capability-Tags. Ein nicht erlaubter UI-, Coroutine- oder Suspend-Aufruf beendet den aktuellen Context mit einer Script-Diagnose. `Async` benötigt das `Coroutine`-Tag. Jede erzeugte Coroutine übernimmt die UI-Capabilities ihres auslösenden Callbacks und erhält zusätzlich immer das `Suspendable`-Tag. Sie darf daher selbst `Utils::Yield`, `Utils::Sleep` und die Wait-Funktionen verwenden, auch wenn der auslösende Callback nicht suspendieren darf. Jeder Ausführungsabschnitt besitzt weiterhin ein Zeitlimit von 100 ms.

Die Deklarationen von `Utils::Yield` und `Utils::Sleep` liegen in `ScriptSuspensionBindings.cpp`. `IScriptSuspensionRuntime` verbindet sie mit der Suspend-/Timer-Logik der Engine.
Gemeinsame Prüfungen von AngelScript-Registrierungsergebnissen und einheitliche Fehlerdiagnosen stellt `ScriptBindingUtils` für alle Binding-Module bereit.

## Tasks und Coroutinen

`Async` startet eine Script-Funktion sofort in einem eigenen Coroutine-Context. Es existieren Überladungen für die Funktion allein und für bis zu neun zusätzliche Argumente:

```angelscript
Core::Task@ Async(?&in function);
Core::Task@ Async(?&in function, ?&in argument1);
// ... bis function + 9 Argumente

void Wait(Core::Task@+ task);
void WaitAll(Core::Task@[] &in tasks);
Core::Task@ WaitAny(Core::Task@[] &in tasks);
```

Die Coroutine darf selbst `Utils::Yield`, `Utils::Sleep` und die Wait-Funktionen verwenden. Der aufrufende Context läuft nach `Async` weiter und wird nur durch einen expliziten Wait-Aufruf suspendiert. `WaitAll` wartet auf alle Tasks; `WaitAny` löst sein zurückgegebenes Task-Handle auf den nach Completion-Reihenfolge zuerst abgeschlossenen Task auf. Das zurückgegebene Handle repräsentiert dessen Status und Resultat, besitzt aber bewusst keine `is`-Identität mit einem Eingabe-Handle.

Die vollständige AngelScript-Deklaration dieser API liegt getrennt von der Engine in `ScriptTaskBindings.cpp`; `IScriptTaskRuntime` verbindet die Bindings nur mit dem Scheduler. Dieses Binding-Modul dient als Muster für weitere getrennte Script-API-Bereiche.

`Core::Task` stellt `IsCompleted`, `IsFailed` und `void Retrieve(?&out)` bereit. Der automatische Cast zu `Core::TypedTask<T>` prüft den Ergebnistyp; der Rückweg zu `Core::Task` ist ebenfalls implizit. Wegen der Regeln für native AngelScript-Templates verwendet der typisierte Task eine Out-Referenz:

```angelscript
funcdef int Calculation(int value);

Calculation@ calculation = @calculate;
Core::Task@ task = Async(calculation, 21);
Wait(task);

Core::TypedTask<int>@ typed = task;
int result;
typed.Retrieve(result);
```

Integer-Ergebnisse werden intern auf `int64`, `float` auf `double` normalisiert. Object- und Handle-Ergebnisse werden bis zur Freigabe des letzten Task-Handles von der Engine referenziert. Das offizielle AngelScript-`scriptarray`-Add-on stellt die Array-Syntax für WaitAll und WaitAny bereit.

Noch nicht implementiert sind Context-Pooling und Include-Auflösung. Details stehen in `docs/PLUGIN_SYSTEM_PLAN.md`.
