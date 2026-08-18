# Render Timeout (Intentional)

Dieses Plugin ist absichtlich fehlerhaft und dient ausschließlich zum Testen der Script-Deadline.

`on_render()` öffnet ein ImGui-Fenster und läuft danach in einer Endlosschleife. Die AngelScript-Engine sollte den
Callback nach 100 ms abbrechen. Anschließend sollte der Host:

1. das noch offene `ImGui::Begin()` automatisch mit `ImGui::End()` schließen,
2. eine Warnung über das fehlende `ui::end_window()` ausgeben,
3. den Deadline-Fehler protokollieren und
4. das Plugin entladen.

Zum manuellen Testen den kompletten Ordner nach `puremirror/plugins/render-timeout` kopieren, PureMirror starten und
im Menü `Plugins > Load > Render Timeout (Intentional)` auswählen.
