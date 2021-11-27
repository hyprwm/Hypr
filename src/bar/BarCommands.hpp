#pragma once
#include "../defines.hpp"
#include "../utilities/Util.hpp"

namespace BarCommands {
    std::string parseCommand(std::string);

    std::string parsePercent(std::string);
    std::string parseDollar(std::string);
};