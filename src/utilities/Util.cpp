#include "Util.hpp"

// Execute a shell command and get the output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "Exec failed in pipe.");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void clearLogs() {
    std::ofstream logs;
    const char* const ENVHOME = getenv("HOME");
    const std::string DEBUGPATH = ENVHOME + (std::string) "/.hypr.log";
    logs.open(DEBUGPATH, std::ios::out | std::ios::trunc);
    logs << " ";
    logs.close();
}