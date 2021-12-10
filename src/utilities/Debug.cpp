#include "Debug.hpp"
#include <fstream>
#include "../windowManager.hpp"

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

    // also log to a file
    g_pWindowManager->DebugOfstream << msg << "\n";
}