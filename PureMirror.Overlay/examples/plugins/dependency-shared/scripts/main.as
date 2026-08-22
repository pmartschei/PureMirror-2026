void OnLoad()
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

void OnRenderInterface()
{
    if (ui::begin_window("Dependency Example - Shared"))
    {
        ui::text("This plugin implements DependencyExample::message().");
    }
    ui::end_window();
}

void OnUnload()
{
    log::info("Shared dependency unloaded");
}
