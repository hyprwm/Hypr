#pragma once

#include <map>
#include "../utilities/Debug.hpp"

struct SConfigValue {
    int intValue = -1;
    float floatValue = -1;
    std::string strValue = "";
};

namespace ConfigManager {
    inline std::map<std::string_view, SConfigValue> configValues;
    inline time_t       lastModifyTime = 0;

    void                init();
    void                loadConfigLoadVars();
    void                tick();

    int                 getInt(std::string_view);
    float               getFloat(std::string_view);
    std::string         getString(std::string_view);
};