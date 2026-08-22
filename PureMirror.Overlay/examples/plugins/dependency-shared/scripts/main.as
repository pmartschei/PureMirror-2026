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
    if (UI::Begin("Dependency Example - Shared"))
    {
        UI::Text("This plugin implements DependencyExample::message().");
    }
    UI::End();
}

void OnUnload()
{
    log::info("Shared dependency unloaded");
}
