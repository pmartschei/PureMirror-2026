void OnLoad()
{
    log::info("Dependency consumer loaded after its shared dependency");
}

void OnRenderInterface()
{
    if (UI::Begin("Dependency Example - Consumer"))
    {
        UI::Text("Loading this plugin automatically loads the shared plugin.");
        UI::Text(DependencyExample::message());
    }
    UI::End();
}

void OnUnload()
{
    log::info("Dependency consumer unloaded");
}
