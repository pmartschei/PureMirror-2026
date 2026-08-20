funcdef int TaskWork(int value, int delay_ms);

string status = "Waiting for on_load";
int first_result = 0;
int slow_result = 0;
bool tasks_completed = false;

int sleep_then_double(int value, int delay_ms)
{
    Utils::Sleep(uint64(delay_ms));
    return value * 2;
}

int yield_then_increment(int value, int unused_delay_ms)
{
    Utils::Yield();
    return value + 1;
}

void on_load()
{
    status = "Running three tasks in parallel...";
    log::info("Async Tasks started");

    TaskWork@ sleeping_work = @sleep_then_double;
    TaskWork@ yielding_work = @yield_then_increment;

    // async starts every function immediately in its own coroutine context.
    // Both timers therefore overlap instead of adding up to 1000 ms.
    Core::Task@ slow = async(sleeping_work, 21, 750);
    Core::Task@ fast = async(sleeping_work, 10, 250);
    Core::Task@ next_frame = async(yielding_work, 6, 0);

    // The Yield coroutine completes first, on the next frame.
    Core::Task@ first = WaitAny({slow, fast, next_frame});
    first.Retrieve(first_result);

    // on_load remains suspended until slow and fast have both completed.
    WaitAll({slow, fast, next_frame});

    // Task automatically casts to TypedTask<int> after its result type is known.
    Core::TypedTask<int>@ typed_slow = slow;
    typed_slow.Retrieve(slow_result);

    tasks_completed = true;
    status = "All tasks completed";
    log::info("First result: " + first_result + ", slow result: " + slow_result);
}

void on_render()
{
    if (ui::begin_window("Async Tasks"))
    {
        ui::text(status);
        ui::text("WaitAny result: " + first_result);
        ui::text("Typed slow result: " + slow_result);
        ui::text(tasks_completed ? "Ready" : "Still working");
    }
    ui::end_window();
}

void on_unload()
{
    log::info("Async Tasks unloaded");
}
