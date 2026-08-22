void OnLoad()
{
    log::info("Cyclic plugin A loaded together with cyclic plugin B");
}

void OnRenderInterface()
{
    if (UI::Begin("Cyclic Dependency Example - A"))
    {
        UI::Text("Plugin A requires B, while B requires A.");
    }
    UI::End();
}

void OnUnload()
{
    log::info("Cyclic plugin A unloaded");
}
