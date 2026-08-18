void on_load()
{
    log::info("Shared dependency loaded");
}

namespace DependencyExample
{
    string message()
    {
        return "This text is implemented by the shared dependency.";
    }
}

void on_render()
{
    if (ui::begin_window("Dependency Example - Shared"))
    {
        ui::text("This plugin implements DependencyExample::message().");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Shared dependency unloaded");
}
