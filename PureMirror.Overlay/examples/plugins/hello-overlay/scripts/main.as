uint click_count = 0;

void OnLoad()
{
    log::info("Hello Overlay loaded");
}

void OnRenderInterface()
{
    if (ui::begin_window("Hello Overlay"))
    {
        ui::text("This UI is rendered by a script plugin.");
        if (ui::button("Count click"))
            click_count++;
        ui::text("Clicks: " + click_count);
    }
    ui::end_window();
}

void OnUnload()
{
    log::info("Hello Overlay unloaded");
}
