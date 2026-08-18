void on_load()
{
    log::info("Cyclic plugin B loaded together with cyclic plugin A");
}

void on_render()
{
    if (ui::begin_window("Cyclic Dependency Example - B"))
    {
        ui::text("Plugin B requires A, while A requires B.");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Cyclic plugin B unloaded");
}
