# PureMirror Overlay: Plugin-System

Status: Architektur und laufende Implementierung  
Script-Engine: AngelScript 2.38.0  
Scope: Script-Plugins; native DLL-Plugins sind bewusst nicht Teil der ersten Version.

## Zielbild

`PureMirror.Overlay` wird zum Host einer kleinen, stabilen Plugin-Plattform. Der Host übernimmt Lebenszyklus, Abhängigkeiten, Logging, Fehlerisolation und die ImGui-Integration. Plugins bleiben normale Dateien in einem Plugin-Verzeichnis und implementieren UI und Logik in Scripts.

Wichtige Grenzen:

- Der Renderer und der ImGui-Kontext gehören immer dem Host.
- Plugins bekommen eine versionierte Script-API, keine rohen C++-Pointer und keine direkte Binärabhängigkeit auf Dear ImGui.
- Jedes Plugin ist ein Paket mit Manifest und mindestens einer Script-Datei.
- Abhängigkeiten werden vor dem Laden validiert; zyklische Komponenten werden gruppiert und der daraus entstehende Graph topologisch sortiert.
- Ein fehlerhaftes Plugin darf andere Plugins und den Render-Loop nicht stoppen.
- Script-Ausführung im Prozess ist Fehlerisolation, aber keine echte Sicherheits-Sandbox.

## Warum AngelScript

AngelScript ist für die erste Version die passendste Wahl:

- Die C++-ähnliche Syntax passt zum Projekt und zu einer unmittelbaren UI-API.
- Host-Funktionen, Objekttypen, Callbacks und Namespaces lassen sich explizit registrieren.
- Getrennte Script-Module entsprechen gut einem Modul pro Plugin.
- Importierte Funktionen können kontrolliert zwischen Modulen gebunden werden.
- Access Masks erlauben später API-Berechtigungen pro Plugin-Kategorie.
- Der ScriptBuilder unterstützt Includes und eine eigene Include-Auflösung.

Lua 5.4 mit sol2 ist die stärkste Alternative, wenn eine größere Community, viele Bibliotheken und besonders schneller Einstieg für Plugin-Autoren wichtiger sind. Lua ist dynamischer und die Binding-Schicht mit sol2 komfortabel, aber API-Fehler werden häufiger erst zur Laufzeit sichtbar. Wren ist klein und gut einzubetten, hat jedoch ein kleineres Ökosystem und eine weniger direkte Abbildung einer großen ImGui-artigen API.

Die Engine bleibt hinter `IScriptEngine` gekapselt. Damit ist die Entscheidung vor Phase 3 noch reversibel. Ein gleichzeitiger Betrieb mehrerer Script-Sprachen ist zunächst ausdrücklich kein Ziel.

## Zielstruktur im Repository

```text
PureMirror.Overlay/
|-- include/
|   `-- PureMirror/Overlay/       # Öffentliche, stabile C++-Host-API
|-- src/
|   |-- app/                      # Zusammensetzen und Lebenszyklus der Overlay-App
|   |-- core/                     # Logger, Events, IDs, Versionen, Result-Typen
|   |-- external/                 # Eingebettete Drittanbieterquellen
|   |-- platform/windows/         # Win32-Input, Pfade, File-Watching
|   |-- plugins/                  # Discovery, Manifest, DAG, Lifecycle, Registry
|   |-- scripting/                # Engine-unabhängige Script-Abstraktion
|   |   `-- angelscript/          # AngelScript-Adapter und Bindings
|   `-- ui/                       # Host-Fenster und Script-UI-Fassade
|       |-- console/              # ImGui Log-Konsole
|       `-- plugin_manager/       # Plugin-Status, Reload, Fehleranzeige
|-- examples/plugins/             # Mitgelieferte Referenz-Plugins
|-- docs/                         # Architektur und Plugin-Autor-Dokumentation
`-- PureMirror.Overlay.vcxproj
```

Bestehende Dateien bleiben vorerst an ihrem Platz. Sie werden in Phase 1 einzeln verschoben und gleichzeitig im `.vcxproj` sowie in `.vcxproj.filters` aktualisiert. So bleibt jeder Schritt baubar.

## Plugin-Verzeichnis zur Laufzeit

```text
puremirror/
|-- plugins/
|   |-- hello-overlay/
|   |   |-- plugin.json
|   |   `-- scripts/main.as
|   `-- another-plugin/
|       |-- plugin.json
|       |-- scripts/main.as
|       `-- assets/
|-- plugin-data/                  # Schreibbarer Zustand, nach Plugin-ID getrennt
|   `-- com.example.hello/
`-- logs/
```

Quellcode, Assets und persistente Daten werden getrennt. Ein Plugin darf ausschließlich in seinem eigenen `plugin-data/<id>`-Verzeichnis schreiben, sofern es die entsprechende Capability besitzt.

## Manifest

Minimalbeispiel:

```json
{
  "schemaVersion": 1,
  "id": "com.example.hello",
  "name": "Hello Overlay",
  "version": "0.1.0",
  "apiVersion": "1.0",
  "entry": "scripts/main.as",
  "exports": ["scripts/public/widgets.as"],
  "dependencies": {
    "com.example.shared": ">=1.2.0 <2.0.0"
  },
  "optionalDependencies": {
    "com.example.shared2": ">=1.2.0 <2.0.0"
  },
  "capabilities": ["ui", "logging"]
}
```

Regeln:

- `id` ist global eindeutig, unveränderlich und wird als Modulname verwendet.
- `schemaVersion` versioniert das Manifestformat; `apiVersion` die Host-API.
- Pluginversionen verwenden zunächst `major.minor.patch`. Versionsbereiche unterstützen `>`, `>=`, `<`, `<=`, `=` und exakte Versionen; mehrere Vergleiche werden mit Leerzeichen verknüpft.
- Pfade sind relativ zum Plugin-Paket. Absolute Pfade und `..` werden abgelehnt.
- Fehlende Pflichtabhängigkeiten verhindern das Laden des betroffenen Teilgraphen und erscheinen in der Konsole.
- Zyklen sind zulässig und werden als gemeinsame Ladegruppe behandelt; fehlende oder versionsinkompatible optionale Abhängigkeiten werden ignoriert.
- `IPluginPackageProvider` abstrahiert lokale Pakete und später vom PureMirror-Server geladene beziehungsweise gecachte Pakete.

## Komponenten

### App

`OverlayApplication` besitzt alle langlebigen Dienste und ersetzt die aktuellen Globals schrittweise. Reihenfolge beim Start:

1. Plattform, Logger und UI-Dienste erzeugen.
2. Script-Engine erzeugen und Host-API registrieren.
3. Plugins entdecken, validieren und in Lade-Reihenfolge bringen.
4. Plugins kompilieren und aktivieren.
5. Pro Frame erst Updates, dann UI rendern.
6. Beim Shutdown Plugins in umgekehrter Reihenfolge deaktivieren.

### Core

- `Logger`: thread-sicherer, begrenzter Speicherpuffer plus optionale Datei-Senke.
- `EventBus`: typisierte Host-Ereignisse; Zustellung auf dem Render-Thread.
- `SemanticVersion`: Vergleich von Host-, Plugin- und Dependency-Versionen.
- `Result/Error`: Fehler mit Code, Plugin-ID und menschenlesbarer Ursache.

Die ImGui-Konsole liest einen Snapshot des Logger-Puffers. Filter nach Level, Plugin-ID und Text sowie Copy/Clear/Auto-scroll gehören zur ersten UI-Version.

### Plugins

- `PluginDiscovery`: findet direkte Unterordner mit `plugin.json`.
- `PluginManifest`: geparste und validierte Metadaten.
- `DependencyResolver`: prüft Pflichtabhängigkeiten, fasst Zyklen zusammen und erzeugt eine Dependency-zuerst-Reihenfolge von Ladegruppen.
- `PluginVersionSolver`: wählt aus lokalen und entfernten Kandidaten eine gemeinsame Version pro Plugin-ID und bevorzugt kompatible installierte Versionen.
- `PluginPackagePlanner`: plant Installation, Update und Entfernung einschließlich transitiver Dependencies und Orphan-Cleanup, ohne den aktiven Zustand direkt zu verändern.
- `PluginReloadPlanner`: berechnet die für einen Reload betroffenen Consumer und zyklischen Gruppen.
- `PluginManager`: alleiniger Besitzer der Zustände `Discovered`, `Loaded`, `Active`, `Failed`, `Disabled`.
- `PluginInstance`: Manifest, Script-Modul, Callback-Handles, Laufzeitfehler und Timing.
- `ServiceRegistry`: kontrollierter Austausch von Hilfsfunktionen zwischen Plugins.

### Scripting

Die generische Grenze soll klein bleiben:

```cpp
class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;
    virtual ScriptLoadResult Load(const PluginManifest& manifest) = 0;
    virtual ScriptCallResult Call(PluginHandle plugin, ScriptCallback callback) = 0;
    virtual void Unload(PluginHandle plugin) = 0;
};
```

Der AngelScript-Adapter besitzt Engine, Module, Context-Pool, Message-Callback, Include-Resolver und alle Bindings. AngelScript-Typen dürfen diesen Ordner nicht verlassen.

Implementierter Stand: AngelScript wird als separates statisches Projekt gebaut. `IScriptEngine` und der Pimpl-Adapter kapseln Engine-Typen; Manifest-`entry` und alle `exports` werden als Script-Sections eines Moduls kompiliert. Compilerdiagnostik enthält Severity, Section, Zeile und Spalte. Die Lifecycle-Callbacks und die minimale `log`-/`ui`-Host-API sind angebunden. Context-Pool, Includes, Capability-Prüfungen, Zeitlimits und Import-Bindung folgen als nächste Schritte.

### UI

Plugins erhalten eine bewusst kuratierte API im Namespace `ui`, zum Beispiel:

```angelscript
namespace ui {
    bool begin_window(const string &in title);
    void end_window();
    void text(const string &in value);
    bool button(const string &in label);
    bool checkbox(const string &in label, bool &inout value);
    void separator();
    void same_line();
}
```

Die Wrapper rufen intern `ImGui::*` auf. Vorteile gegenüber einem vollständigen Direkt-Export:

- stabile API trotz Dear-ImGui-Upgrades,
- Validierung von Strings, IDs und Begin/End-Paaren,
- messbares Zeitbudget pro Plugin,
- später automatisch generierbare API-Dokumentation,
- keine Exposition von `ImGuiContext`, `ImDrawList` oder internen Strukturen.

Begin/End-Paare werden zusätzlich vom Host verfolgt und nach einem Script-Fehler soweit möglich bereinigt. Low-Level-Zeichnen und Texturen folgen erst nach einer geklärten Ressourcen-API.

## Plugin-Vertrag

Reservierte, optionale Einstiegspunkte:

```angelscript
void on_load();
void on_unload();
void on_update(float delta_seconds);
void on_render();
```

- `on_load` läuft genau einmal nach erfolgreich gebundenen Dependencies.
- `on_update` und `on_render` laufen nur für aktive Plugins auf dem Render-Thread.
- `on_unload` läuft bei geordnetem Shutdown oder Reload, aber nicht als Garantie nach einem schweren Prozessfehler.
- Fehlt ein Callback, ist das gültig.
- Exceptions werden mit Plugin-ID und Stack-Information protokolliert; das Plugin wird für den Rest des Frames deaktiviert und nach konfigurierter Fehlergrenze dauerhaft pausiert.

## Abhängigkeiten und exportierte Hilfsfunktionen

Das Manifest ist die einzige Quelle für Abhängigkeiten. Nur deklarierte Dependencies dürfen konsumiert werden.

Für Version 1 gibt es zwei Wege:

1. AngelScript-Funktionsimporte für kleine, stateless Hilfsfunktionen. Der Host prüft die Deklaration und bindet den Import erst nach erfolgreichem Laden des Providers.
2. Eine Host-verwaltete `ServiceRegistry` für längerlebige APIs. Ein Provider registriert eine benannte, versionierte Script-Schnittstelle; Consumer fragen sie ausschließlich über eine deklarierte Dependency ab.

Geteilte veränderliche globale Variablen werden nicht unterstützt. Daten werden als Werte, Handles auf ausdrücklich freigegebene Host-Objekte oder Events übertragen. Plugin-Namespaces und Service-Namen basieren immer auf der Plugin-ID, damit keine zufälligen Kollisionen entstehen.

## Fehler-, Ressourcen- und Sicherheitsmodell

- Kein Plugin-Aufruf darf eine C++-Exception über die Host-Grenze tragen.
- Pro Callback werden Laufzeit und Fehler gezählt; langsame Plugins werden im Plugin Manager sichtbar.
- Eine AngelScript-Line-Callback kann ein Zeit- oder Instruktionsbudget durchsetzen.
- Includes dürfen das Plugin-Verzeichnis und explizit freigegebene Dependency-Exports nicht verlassen.
- Direkter Prozess-, Netzwerk-, Registry- und beliebiger Dateisystemzugriff wird nicht registriert.
- ImGui-Aufrufe sind ausschließlich während `on_render` erlaubt.
- Plugin-Reload erfolgt zunächst manuell. Automatisches File-Watching kommt erst, wenn Unload und State-Verlust zuverlässig getestet sind.
- Native DLL-Plugins wären vollständig vertrauenswürdig und könnten den Prozess kompromittieren; dafür wäre später eine separate, versionierte C-ABI nötig.
- Paketänderungen werden zuerst vollständig aufgelöst und als Plan angezeigt. Downloads landen in einem temporären Bereich; erst nach erfolgreicher Validierung wird der neue Lock-Zustand atomar aktiviert.

## Umsetzung in Phasen

### Phase 1: Host aufteilen

- `OverlayApplication` als Composition Root einführen.
- Bestehende Queue-UI nach `src/ui` und Win32-Code nach `src/platform/windows` verschieben.
- Globals durch klar besessene Dienste ersetzen.
- Bestehende Tests nach jedem Verschieben weiterbauen.

Abnahme: Verhalten unverändert, kein Plugin-Code vorhanden, bestehende Tests grün.

### Phase 2: Core und Konsole

- Logger mit Ringpuffer und Levels `Trace`, `Debug`, `Info`, `Warning`, `Error`.
- Datei-Senke und ImGui-Konsole implementieren.
- Plugin-ID als strukturierte Log-Quelle vorsehen.

Abnahme: Logs aus mehreren Threads erscheinen geordnet und filterbar in ImGui.

### Phase 3: AngelScript-Minimum

- AngelScript als klar versionierte Drittanbieterabhängigkeit hinzufügen.
- `IScriptEngine`, Adapter, Context-Pool und Compiler-Diagnostik implementieren.
- Nur `logging` und eine minimale `ui`-API binden.
- Referenz-Plugin laden und rendern.

Abnahme: Ein Script kann ein Fenster anzeigen; Syntax- und Laufzeitfehler bleiben lokal und landen in der Konsole.

### Phase 4: Pakete und Dependencies

- JSON-Manifestparser, Schema-Prüfung und sichere Pfadauflösung.
- SemVer-Bereiche ergänzen, zyklische Ladegruppen bilden und deren Graph topologisch sortieren.
- Imports und Service Registry implementieren.

Abnahme: Tests für fehlende Dependency, falsche Version, Pflicht- und optionale Zyklen, deterministische Gruppenreihenfolge und Export/Import.

### Phase 5: Betrieb

- Plugin-Manager-Fenster mit Status, Fehlern, Enable/Disable und manuellem Reload.
- Zeitbudget, Fehlergrenze und Telemetrie pro Callback.
- Persistente Plugin-Konfiguration unter `plugin-data/<id>`.

Abnahme: Ein Plugin kann wiederholt geladen und entladen werden, ohne Callback-, Context- oder Ressourcen-Leaks.

## Tests, die früh angelegt werden sollten

- Manifest: gültig, unbekannte Felder, ungültige ID, Pfad-Traversal, API-Inkompatibilität.
- Resolver: lineare und diamantförmige Abhängigkeiten, fehlende Version, Self-Cycle und Multi-Cycle.
- Lifecycle: Callback-Reihenfolge, partielles Scheitern, Shutdown in umgekehrter Reihenfolge.
- Script-Adapter: Compilerfehler, Exception, Timeout, fehlender Callback und Reload.
- UI-Fassade: Aufruf außerhalb `on_render`, unausgeglichenes Begin/End und ID-Kollisionen.
- Logger: parallele Producer, Puffergrenze, Filter und lange Nachrichten.

## Vor Implementierungsbeginn zu entscheiden

1. Sind Plugins ausschließlich lokal und vertrauenswürdig, oder werden fremde Plugins verteilt? Das bestimmt, ob In-Process-Scripting genügt.
2. Soll Plugin-State bei Reload erhalten bleiben? Falls ja, braucht Version 1 bereits `serialize_state`/`restore_state`.
3. Sollen Plugins nur Overlay-UI und Host-Events sehen, oder auch Spiel-/Renderer-spezifische Daten?
4. Ist `puremirror/plugins` relativ zur Host-DLL der gewünschte Installationsort?
5. Soll es langfristig auch native DLL-Plugins geben? Deren ABI muss getrennt von der Script-API geplant werden.

## Empfohlener erster vertikaler Schnitt

Nach Phase 1 und 2 nicht sofort die komplette ImGui-API binden. Der erste End-to-End-Schnitt sollte nur Folgendes enthalten:

- ein Manifest,
- Dependency-freies Laden eines `.as`-Scripts,
- `log.info`,
- `ui.begin_window`, `ui.text`, `ui.button`, `ui.end_window`,
- `on_load`, `on_render`, `on_unload`,
- sichtbarer Status in der Host-Konsole.

Damit werden Pfade, Ownership, Callbacks, Fehlerbehandlung und Rendering einmal vollständig bewiesen, bevor die API-Fläche wächst.
