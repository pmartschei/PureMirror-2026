void OnLoad()
{
    log::info("Intentional render-timeout example loaded");
}

void OnRenderInterface()
{
    const bool is_visible = UI::Begin("Intentional Render Timeout");
    if (is_visible)
    {
        UI::Text("This plugin intentionally exceeds the 100 ms render deadline.");
        UI::Text("The host should abort it, close this window scope and unload the plugin.");
    }

    while (true)
    {
    }

    // Intentionally unreachable. The host must recover the open window scope.
    UI::End();
}

void OnUnload()
{
    log::info("Intentional render-timeout example unloaded");
}
