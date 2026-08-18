void on_load()
{
    log::info("Dependency consumer loaded after its shared dependency");
}

void on_render()
{
    if (ui::begin_window("Dependency Example - Consumer"))
    {
        ui::text("Loading this plugin automatically loads the shared plugin.");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Dependency consumer unloaded");
}
