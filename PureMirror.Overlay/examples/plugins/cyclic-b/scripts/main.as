void OnLoad()
{
    log::info("Cyclic plugin B loaded together with cyclic plugin A");
}

void OnRenderInterface()
{
    if (ui::begin_window("Cyclic Dependency Example - B"))
    {
        ui::text("Plugin B requires A, while A requires B.");
    }
    ui::end_window();
}

void OnUnload()
{
    log::info("Cyclic plugin B unloaded");
}
