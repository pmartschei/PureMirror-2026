# Scripting

Script-Engine-unabhängige Verträge, Handles, Callback-Ergebnisse und Host-API-Registrierung.

`IScriptEngine` lädt benannte Module aus beliebig vielen `ScriptSource`-Abschnitten und liefert strukturierte Compilerdiagnostik zurück. `PluginScriptCompiler` liest den Manifest-Einstiegspunkt und alle Exporte aus einem kanonisch geprüften Paketpfad und kompiliert sie als gemeinsames Modul unter der Plugin-ID.

Konkrete Engines werden in einem eigenen Unterordner implementiert. Die erste Implementierung liegt unter `angelscript/`.
