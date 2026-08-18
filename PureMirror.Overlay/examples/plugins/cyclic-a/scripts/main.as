void on_load()
{
    log::info("Cyclic plugin A loaded together with cyclic plugin B");
}

void on_render()
{
    if (ui::begin_window("Cyclic Dependency Example - A"))
    {
        ui::text("Plugin A requires B, while B requires A.");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Cyclic plugin A unloaded");
}
