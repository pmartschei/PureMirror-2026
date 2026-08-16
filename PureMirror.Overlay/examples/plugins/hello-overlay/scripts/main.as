bool window_open = true;
uint click_count = 0;

void on_load()
{
    log::info("Hello Overlay loaded");
}

void on_render()
{
    if (!window_open)
        return;

    if (ui::begin_window("Hello Overlay", window_open))
    {
        ui::text("This UI is rendered by a script plugin.");
        if (ui::button("Count click"))
            click_count++;
        ui::text("Clicks: " + click_count);
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Hello Overlay unloaded");
}
