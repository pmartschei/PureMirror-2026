void on_load()
{
    log::info("Shared dependency loaded");
}

void on_render()
{
    if (ui::begin_window("Dependency Example - Shared"))
    {
        ui::text("This plugin provides the shared example dependency.");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Shared dependency unloaded");
}
