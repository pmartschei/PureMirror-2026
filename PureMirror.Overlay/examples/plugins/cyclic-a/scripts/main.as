void OnLoad()
{
    log::info("Cyclic plugin A loaded together with cyclic plugin B");
}

void OnRenderInterface()
{
    if (ui::begin_window("Cyclic Dependency Example - A"))
    {
        ui::text("Plugin A requires B, while B requires A.");
    }
    ui::end_window();
}

void OnUnload()
{
    log::info("Cyclic plugin A unloaded");
}
