uint click_count = 0;

void OnLoad()
{
    log::info("Hello Overlay loaded");
}

void OnRenderInterface()
{
    if (UI::Begin("Hello Overlay"))
    {
        UI::Text("This UI is rendered by a script plugin.");
        if (UI::Button("Count click"))
            click_count++;
        UI::Text("Clicks: " + click_count);
    }
    UI::End();
}

void OnUnload()
{
    log::info("Hello Overlay unloaded");
}
