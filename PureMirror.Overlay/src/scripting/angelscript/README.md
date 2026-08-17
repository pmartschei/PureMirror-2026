# AngelScript Adapter

Der Adapter verwendet AngelScript 2.38.0 als statisch gelinkte Bibliothek. Er besitzt die Engine, registriert den offiziellen `std::string`-Typ, kompiliert mehrere Script-Sections zu einem Modul, sammelt strukturierte Diagnostik und kann Module wieder entladen.

AngelScript-Header und -Typen bleiben durch ein Pimpl vollständig in diesem Ordner. Der engine-unabhängige Layer kennt ausschließlich `IScriptEngine`, `ScriptSource` und `ScriptModuleLoadResult`.

Implementiert sind außerdem die optionalen Lifecycle-Callbacks `on_load`, `on_render` und `on_unload` sowie die minimale Host-API `log::info`, `ui::begin_window`, `ui::end_window`, `ui::text` und `ui::button`. `PluginScriptInstance` verbindet Manifest, Paketpfad, Kompilierung, Callback-Ausführung und Entladen zu einem Plugin-Lifecycle.

Noch nicht implementiert sind Context-Pool, Include-Auflösung, das Binden von Imports zwischen Dependency-Ladegruppen, Capability-Prüfungen und eine Zeitbegrenzung für Scripts. Details stehen in `docs/PLUGIN_SYSTEM_PLAN.md`.
