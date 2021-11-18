#pragma once
#include <string>

enum LogLevel {
    NONE = -1,
    LOG = 0,
    WARN,
    ERR,
    CRIT
};

namespace Debug {
    void                log(LogLevel, std::string msg);
};