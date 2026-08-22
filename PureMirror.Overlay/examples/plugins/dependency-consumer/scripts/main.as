void OnLoad()
{
    log::info("Dependency consumer loaded after its shared dependency");
}

void OnRenderInterface()
{
    if (ui::begin_window("Dependency Example - Consumer"))
    {
        ui::text("Loading this plugin automatically loads the shared plugin.");
        ui::text(DependencyExample::message());
    }
    ui::end_window();
}

void OnUnload()
{
    log::info("Dependency consumer unloaded");
}
