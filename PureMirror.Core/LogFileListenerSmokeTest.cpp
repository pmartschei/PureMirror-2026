#include "LogFileListener.h"

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

using namespace std::chrono_literals;

int main()
{
    const auto path = std::filesystem::path{"LogFileListenerSmokeTest.log"};
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "CHAT [Old]: ignored\n";
    }

    PureMirror::LogFileListener listener(path, {.PollInterval = 10ms});
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::string> messages;
    listener.SetErrorCallback([](const std::string& error) { std::cerr << error << '\n'; });

    listener.Subscribe(std::regex{R"(CHAT \[([^]]+)\]: (.+))"}, [&](const std::string&, const std::smatch& match) {
        {
            std::scoped_lock lock(mutex);
            messages.push_back(match[1].str() + ":" + match[2].str());
            std::cerr << "matched " << messages.back() << '\n';
        }
        changed.notify_all();
    });

    listener.Start();
    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        file << "CHAT [Alice]: hel";
    }
    std::this_thread::sleep_for(40ms);
    {
        std::scoped_lock lock(mutex);
        if (!messages.empty())
            return 1;
    }
    {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        file << "lo\r\nNOT CHAT\n";
    }
    {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 1s, [&] { return messages.size() == 1; }))
            return 2;
    }

    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "CHAT [Bob]: rotated\n";
    }
    {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, 1s, [&] { return messages.size() == 2; }))
            return 3;
        if (messages[0] != "Alice:hello" || messages[1] != "Bob:rotated")
            return 4;
    }

    listener.Stop();
    std::filesystem::remove(path);
    std::cout << "LogFileListener smoke test passed\n";
    return 0;
}
