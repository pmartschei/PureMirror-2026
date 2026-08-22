void OnLoad()
{
    log::info("Cyclic plugin B loaded together with cyclic plugin A");
}

void OnRenderInterface()
{
    if (UI::Begin("Cyclic Dependency Example - B"))
    {
        UI::Text("Plugin B requires A, while A requires B.");
    }
    UI::End();
}

void OnUnload()
{
    log::info("Cyclic plugin B unloaded");
}
