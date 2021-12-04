#pragma once

#include <map>
#include "../utilities/Debug.hpp"

enum ELayouts {
    LAYOUT_DWINDLE = 0,
    LAYOUT_MASTER
};

struct SConfigValue {
    int64_t intValue = -1;
    float floatValue = -1;
    std::string strValue = "";
};

namespace ConfigManager {
    inline std::map<std::string_view, SConfigValue> configValues;
    inline time_t       lastModifyTime = 0;

    inline bool         loadBar = false;

    inline bool         isBar = false;  // If true we send the command to the bar parser

    inline bool         isFirstLaunch = false;

    void                init();
    void                loadConfigLoadVars();
    void                tick();

    void                applyKeybindsToX();

    int                 getInt(std::string_view);
    float               getFloat(std::string_view);
    std::string         getString(std::string_view);
};