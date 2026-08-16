#pragma once

#include <functional>
#include <regex>
#include <string>

namespace PureMirror
{
    using LogFileMatchCallback = std::function<void(const std::string& line, const std::smatch& match)>;

    struct LogFileSubscription
    {
        std::regex Pattern;
        LogFileMatchCallback Callback;
    };
}  // namespace PureMirror
