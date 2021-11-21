#include "Debug.hpp"
#include <fstream>

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
    const char* const ENVHOME = getenv("HOME");

    const std::string DEBUGPATH = ENVHOME + (std::string) "/.hypr.log";
    std::ofstream ofs;

    ofs.open(DEBUGPATH, std::ios::out | std::ios::app);

    ofs << msg << "\n";

    ofs.close();
}