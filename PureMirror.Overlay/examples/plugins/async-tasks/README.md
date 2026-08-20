# Async Tasks

Dieses Beispiel zeigt die Task- und Coroutine-API in einem vollständigen Plugin-Lifecycle:

- Zwei Aufrufe von `sleep_then_double` starten mit `async` in getrennten Coroutine-Contexts und schlafen 750 ms
  beziehungsweise 250 ms. Die Wartezeiten überlappen sich.
- `yield_then_increment` wird ebenfalls parallel gestartet und läuft nach `Utils::Yield()` im nächsten Frame weiter.
- `WaitAny` pausiert `on_load`, bis der erste Task fertig ist, und gibt dessen Resultat-Task zurück.
- `WaitAll` pausiert `on_load` danach, bis alle drei Tasks abgeschlossen sind.
- Der automatische Cast von `Core::Task` zu `Core::TypedTask<int>` erlaubt das typisierte Auslesen des Ergebnisses.

`on_render` verwendet ausschließlich die UI-API. Die Coroutine- und Wait-Aufrufe stehen bewusst in `on_load`, weil
`on_render` nicht suspendierbar ist. Solange `on_load` wartet, wird der Render-Callback dieses Plugins noch nicht
aufgerufen; andere geladene Plugins werden währenddessen normal weiter ausgeführt.

Zum manuellen Testen den kompletten Ordner nach `puremirror/plugins/async-tasks` kopieren, PureMirror starten und im
Menü `Plugins > Load > Async Tasks` auswählen. Nach ungefähr 750 ms zeigt das Fenster als erstes Ergebnis `7` und
als typisiertes Ergebnis des langsamen Tasks `42` an.
