void on_load()
{
    log::info("Intentional render-timeout example loaded");
}

void on_render()
{
    const bool is_visible = ui::begin_window("Intentional Render Timeout");
    if (is_visible)
    {
        ui::text("This plugin intentionally exceeds the 100 ms render deadline.");
        ui::text("The host should abort it, close this window scope and unload the plugin.");
    }

    while (true)
    {
    }

    // Intentionally unreachable. The host must recover the open window scope.
    ui::end_window();
}

void on_unload()
{
    log::info("Intentional render-timeout example unloaded");
}
