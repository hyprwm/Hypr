#include "BarCommands.hpp"
#include "../windowManager.hpp"

std::vector<std::pair<int, int>> lastReads;

std::string getCpuString() {
    std::vector<std::pair<int, int>> usageRead;

    std::ifstream cpuif;
    cpuif.open("/proc/stat");

    std::string line;
    while (std::getline(cpuif, line)) {
        // parse line
        const auto stats = splitString(line, ' ');

        if (stats.size() < 1)
            continue;

        if (stats[0].find("cpu") == std::string::npos)
            break;

        // get the percent
        try {
            const auto user = stol(stats[1]);
            const auto nice = stol(stats[2]);
            const auto kern = stol(stats[3]);

            // get total
            long total = 0;
            for (const auto& t : stats) {
                if (t.find("c") != std::string::npos)
                    continue;

                total += stol(t);
            }

            usageRead.push_back({user + nice + kern, total});
        } catch( ... ) { ; } // oops
    }

    // Compare the values
    std::pair<int, int> lastReadsTotal = {0, 0};
    for (const auto& lr : lastReads) {
        lastReadsTotal.first += lr.first;
        lastReadsTotal.second += lr.second;
    }

    std::pair<int, int> newReadsTotal = {0, 0};
    for (const auto& nr : usageRead) {
        newReadsTotal.first += nr.first;
        newReadsTotal.second += nr.second;
    }

    std::pair<int, int> difference = {newReadsTotal.first - lastReadsTotal.first, newReadsTotal.second - lastReadsTotal.second};

    float percUsage = (float)difference.first / (float)difference.second;

    lastReads.clear();

    for (const auto& nr : usageRead) {
        lastReads.push_back(nr);
    }

    return std::to_string((int)(percUsage * 100.f)) + "%";
}

std::string getRamString() {
    float available = 0;
    float total = 0;

    std::ifstream ramif;
    ramif.open("/proc/meminfo");

    std::string line;
    while (std::getline(ramif, line)) {
        // parse line
        const auto KEY = line.substr(0, line.find_first_of(':'));

        int startValue = 0;
        for (int i = 0; i < line.length(); ++i) {
            const auto& c = line[i];
            if (c >= '0' && c <= '9') {
                startValue = i;
                break;
            }
        }

        float VALUE = 0.f;
        try {
            std::string toVal = line.substr(startValue);
            toVal = toVal.substr(0, line.find_first_of(' ', startValue));
            VALUE = stol(toVal);
        } catch (...) { ; } // oops
        VALUE /= 1024.f;

        if (KEY == "MemTotal") {
            total = VALUE;
        } else if (KEY == "MemAvailable") {
            available = VALUE;
        }
    }

    return std::to_string((int)(total - available)) + "MB/" + std::to_string((int)total) + "MB";
}

std::string getCurrentWindowName() {
    return g_pWindowManager->statusBar->getLastWindowName();
}

std::string getCurrentWindowClass() {
    return g_pWindowManager->statusBar->getLastWindowClass();
}

std::string BarCommands::parsePercent(std::string token) {
    // check what the token is and act accordingly.

    if (token == "RAM") return getRamString();
    else if (token == "CPU") return getCpuString();
    else if (token == "WINNAME") return getCurrentWindowName();
    else if (token == "WINCLASS") return getCurrentWindowClass();

    Debug::log(ERR, "Unknown token while parsing module: %" + token + "%");

    return "Error";
}

std::string BarCommands::parseDollar(std::string token) {
    const auto result = exec(token.c_str());
    return result.substr(0, result.length() - 1);
}

std::string BarCommands::parseCommand(std::string command) {

    std::string result = "";

    for (long unsigned int i = 0; i < command.length(); ++i) {

        const auto c = command[i];

        if (c == '%') {
            // find the next one
            for (long unsigned int j = i + 1; i < command.length(); ++j) {
                if (command[j] == '%') {
                    // found!
                    auto toSend = command.substr(i + 1);
                    toSend = toSend.substr(0, toSend.find_first_of('%'));
                    result += parsePercent(toSend);
                    i = j;
                    break;
                }

                if (command[j] == ' ')
                    break; // if there is a space it's not a token
            }
        }

        else if (c == '$') {
            // find the next one
            for (long unsigned int j = i + 1; i < command.length(); ++j) {
                if (command[j] == '$' && command[j - 1] != '\\') {
                    // found!
                    auto toSend = command.substr(i + 1, j - (i + 1));
                    std::string toSendWithRemovedEscapes = "";
                    for (std::size_t k = 0; k < toSend.length(); ++k) {
                        if (toSend[k] == '\\' && (k + 1) < toSend.length()) {
                            char next = toSend[k + 1];
                            if (next == '$' || next == '{' || next == '}') {
                                continue;
                            }
                        } 
                        toSendWithRemovedEscapes += toSend[k];
                    }
                    result += parseDollar(toSendWithRemovedEscapes);
                    i = j;
                    break;
                }

                if (j + 1 == command.length()) {
                    Debug::log(ERR, "Unpaired $ in a module, module command: ");
                    Debug::log(NONE, command);
                }
            }
        }

        else {
            result += command[i];
        }
    }

    return result;
}