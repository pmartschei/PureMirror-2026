# Plugins

Dieser Layer kennt die abstrakte Script-Schnittstelle, aber keine AngelScript-Typen und keine direkten ImGui-Aufrufe.

## Manifest V1

- `entry` ist das private Einstiegsscript eines Plugins.
- `exports` enthält beliebig viele paketrelative `.as`-Quelldateien, die Dependencies später importieren dürfen.
- `dependencies` ordnet den IDs zwingend benötigter Plugins jeweils einen Versionsbereich zu.
- `optionalDependencies` hat dieselbe Form, wird aber nur berücksichtigt, wenn eine kompatible Version verfügbar ist.
- Ein Plugin darf sich nicht selbst referenzieren. Zyklen zwischen mehreren Plugins sind sowohl für Pflicht- als auch optionale Dependencies erlaubt.

Der Resolver fasst Zyklen in einer gemeinsamen Ladegruppe zusammen. Innerhalb einer solchen Gruppe müssen die spätere Script-Engine zunächst alle Module deklarieren und danach ihre Imports binden. Gruppen selbst stehen in Dependency-zuerst-Reihenfolge.

`IPluginPackageProvider` trennt die Paketquelle von Parsing und Auflösung. Ein lokaler Provider kann Verzeichnisse finden; ein späterer Server-Provider kann Pakete herunterladen beziehungsweise cachen und dieselben `PluginPackage`-Objekte liefern.

Versionsbereiche bestehen in V1 aus durch Leerzeichen verknüpften Vergleichen wie `>=1.2.0 <2.0.0`. Unterstützt sind `>`, `>=`, `<`, `<=`, `=` und eine exakte Version. Pluginversionen verwenden zunächst strikt `major.minor.patch`; Pre-Releases sowie `^`, `~` und OR-Ausdrücke sind noch nicht Teil des Formats.

## Paketplanung

`PluginVersionSolver` wählt aus allen lokalen und entfernten Paketen genau eine Version pro Plugin-ID. Er bevorzugt eine bereits installierte Version, solange sie weiterhin alle Anforderungen erfüllt, und probiert bei Konflikten andere Plugin- und Dependency-Versionen aus. Pflichtabhängigkeiten werden transitiv ausgewählt; optionale Dependencies erzwingen keine Installation.

`PluginPackagePlanner` erzeugt deklarative Pläne und verändert selbst weder Dateien noch den laufenden Host:

- `PlanInstall` ergänzt ein explizit gewünschtes Plugin und alle transitiven Dependencies.
- `PlanRemove` entfernt ein explizites Plugin und nicht mehr benötigte automatische Dependencies.
- Direktes Entfernen einer weiterhin benötigten Dependency wird abgelehnt.
- `PluginChangePlan` enthält Install-, Update- und Remove-Operationen, den neuen Lock-Zustand und die Ladegruppen.

Der Aufrufer zeigt diesen Plan an, lädt benötigte Remote-Pakete in einen temporären Bereich, validiert sie und übernimmt den Plan erst danach atomar. Bei einem Fehler bleibt der bisherige `PluginInstallation`-Zustand aktiv.

`PluginReloadPlanner` ermittelt für einen Reload das Ziel, alle direkten und transitiven Consumer sowie vollständige zyklische Ladegruppen. Entladen wird in umgekehrter, Laden in normaler Dependency-Reihenfolge.

## Runtime-Manager

`PluginManager` scannt beim Overlay-Start den Ordner `puremirror/plugins` relativ zur geladenen Overlay-DLL. Jeder direkte Unterordner mit einer `plugin.json` ist ein lokales Paket. Der Versionssolver wählt bei mehreren lokalen Versionen eine kompatible Version je Plugin-ID; anschließend validiert der Dependency-Resolver die Auswahl und bestimmt die Ladegruppen.

Erst danach kompiliert der Manager die ausgewählten Scripts und führt `on_load` aus. Während jedes Overlay-Frames wird `on_render` aufgerufen. Ein Plugin mit einem Laufzeitfehler wird protokolliert und entladen; beim Abbau des Managers laufen `on_unload` und das Entladen in umgekehrter Reihenfolge.
