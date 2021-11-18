#include "Debug.hpp"

void Debug::log(LogLevel level, std::string msg) {
    switch (level)
    {
        case LOG:
            msg = "[LOG] " + msg;
            break;

        case WARN:
            msg = "[WARN] " + msg;
            break;

        case ERR:
            msg = "[ERR] " + msg;
            break;

        case CRIT:
            msg = "[CRITICAL] " + msg;
            break;

        default:
            break;
    }
    printf((msg + "\n").c_str());
}