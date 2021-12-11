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
    printf("%s", (msg + "\n").c_str());

    // also log to a file
    const std::string DEBUGPATH = ISDEBUG ? "/tmp/hypr/hyprd.log" : "/tmp/hypr/hypr.log";
    std::ofstream ofs;
    ofs.open(DEBUGPATH, std::ios::out | std::ios::app);
    ofs << msg << "\n";
    ofs.close();
}
