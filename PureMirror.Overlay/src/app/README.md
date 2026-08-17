# App

Composition Root und Lebenszyklus von `PureMirror.Overlay`.

Die aktuelle Composition Root liegt noch in `PureMirror.Overlay.cpp`. Dort werden Logger, AngelScript-Host, AngelScript-Engine und `PluginManager` erzeugt. Beim Initialisieren lädt der Manager lokale Plugins aus `puremirror/plugins` relativ zur Overlay-DLL; der Render-Callback führt danach in jedem Frame die geladenen Plugin-Scripts aus.

Eine spätere `OverlayApplication` soll diese globalen Dienste übernehmen und von der schmalen exportierten `OverlayAPI` aufgerufen werden. Fachlogik und konkrete UI gehören nicht in diesen Ordner.
