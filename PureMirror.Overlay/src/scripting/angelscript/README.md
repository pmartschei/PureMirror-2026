# AngelScript Adapter

Der Adapter verwendet AngelScript 2.38.0 als statisch gelinkte Bibliothek. Er besitzt die Engine, registriert den offiziellen `std::string`-Typ, kompiliert mehrere Script-Sections zu einem Modul, sammelt strukturierte Diagnostik und kann Module wieder entladen.

AngelScript-Header und -Typen bleiben durch ein Pimpl vollständig in diesem Ordner. Der engine-unabhängige Layer kennt ausschließlich `IScriptEngine`, `ScriptSource` und `ScriptModuleLoadResult`.

Implementiert sind außerdem die optionalen Lifecycle-Callbacks `on_load`, `on_render` und `on_unload` sowie die minimale Host-API `log::info`, `ui::begin_window`, `ui::end_window`, `ui::text` und `ui::button`. `PluginScriptInstance` verbindet Manifest, Paketpfad, Kompilierung, Callback-Ausführung und Entladen zu einem Plugin-Lifecycle.

Callbacks tragen engine-unabhängige Capability-Tags. `on_load` ist pausierbar, `on_render` darf UI verwenden und `on_unload` besitzt keine dieser Capabilities. Ein Script kann in pausierbaren Callbacks mit `Utils::Yield()` bis zum nächsten Frame oder mit `Utils::Sleep(uint64 timeInMs)` mindestens bis zum Ablauf der angegebenen Zeit aussetzen. Ein nicht erlaubter UI- oder Suspend-Aufruf beendet den Callback mit einer Script-Diagnose. Jeder Ausführungsabschnitt besitzt weiterhin ein Zeitlimit von 100 ms.

Noch nicht implementiert sind Context-Pooling, Include-Auflösung und allgemeine Script-Coroutinen. Details stehen in `docs/PLUGIN_SYSTEM_PLAN.md`.
