# AngelScript Adapter

Der Adapter verwendet AngelScript 2.38.0 als statisch gelinkte Bibliothek. Er besitzt die Engine, registriert den offiziellen `std::string`-Typ, kompiliert mehrere Script-Sections zu einem Modul, sammelt strukturierte Diagnostik und kann Module wieder entladen.

AngelScript-Header und -Typen bleiben durch ein Pimpl vollständig in diesem Ordner. Der engine-unabhängige Layer kennt ausschließlich `IScriptEngine`, `ScriptSource` und `ScriptModuleLoadResult`.

Implementiert sind außerdem die optionalen Lifecycle-Callbacks `on_load`, `on_render` und `on_unload` sowie die minimale Host-API `log::info`, `ui::begin_window`, `ui::end_window`, `ui::text` und `ui::button`. `PluginScriptInstance` verbindet Manifest, Paketpfad, Kompilierung, Callback-Ausführung und Entladen zu einem Plugin-Lifecycle.

Callbacks tragen engine-unabhängige Capability-Tags. `on_load` ist pausierbar und darf Coroutinen starten, `on_render` darf UI verwenden und `on_unload` besitzt keine dieser Capabilities. Ein Script kann in pausierbaren Callbacks mit `Utils::Yield()` bis zum nächsten Frame oder mit `Utils::Sleep(uint64 timeInMs)` mindestens bis zum Ablauf der angegebenen Zeit aussetzen. `async` benötigt das `Coroutine`-Tag; die gestartete Coroutine erbt exakt alle Tags ihres aufrufenden Contexts. Dadurch bleiben verschachtelte Coroutinen erlaubt, erhalten aber beispielsweise aus `on_load` keine UI-Berechtigung. Ein nicht erlaubter UI-, Coroutine- oder Suspend-Aufruf beendet den aktuellen Context mit einer Script-Diagnose. Jeder Ausführungsabschnitt besitzt weiterhin ein Zeitlimit von 100 ms.

## Tasks und Coroutinen

`async` startet eine Script-Funktion sofort in einem eigenen Coroutine-Context. Es existieren Überladungen für die Funktion allein und für bis zu neun zusätzliche Argumente:

```angelscript
Core::Task@ async(?&in function);
Core::Task@ async(?&in function, ?&in argument1);
// ... bis function + 9 Argumente

void Wait(Core::Task@+ task);
void WaitAll(Core::Task@[] &in tasks);
Core::Task@ WaitAny(Core::Task@[] &in tasks);
```

Die Coroutine darf selbst `Utils::Yield`, `Utils::Sleep` und die Wait-Funktionen verwenden. Der aufrufende Context läuft nach `async` weiter und wird nur durch einen expliziten Wait-Aufruf suspendiert. `WaitAll` wartet auf alle Tasks; `WaitAny` löst sein zurückgegebenes Task-Handle auf den nach Completion-Reihenfolge zuerst abgeschlossenen Task auf. Das zurückgegebene Handle repräsentiert dessen Status und Resultat, besitzt aber bewusst keine `is`-Identität mit einem Eingabe-Handle.

`Core::Task` stellt `IsCompleted`, `IsFailed` und `void Retrieve(?&out)` bereit. Der automatische Cast zu `Core::TypedTask<T>` prüft den Ergebnistyp; der Rückweg zu `Core::Task` ist ebenfalls implizit. Wegen der Regeln für native AngelScript-Templates verwendet der typisierte Task eine Out-Referenz:

```angelscript
funcdef int Calculation(int value);

Calculation@ calculation = @calculate;
Core::Task@ task = async(calculation, 21);
Wait(task);

Core::TypedTask<int>@ typed = task;
int result;
typed.Retrieve(result);
```

Integer-Ergebnisse werden intern auf `int64`, `float` auf `double` normalisiert. Object- und Handle-Ergebnisse werden bis zur Freigabe des letzten Task-Handles von der Engine referenziert. Das offizielle AngelScript-`scriptarray`-Add-on stellt die Array-Syntax für WaitAll und WaitAny bereit.

Noch nicht implementiert sind Context-Pooling und Include-Auflösung. Details stehen in `docs/PLUGIN_SYSTEM_PLAN.md`.
