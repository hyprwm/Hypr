#pragma once

#include <map>
#include "../utilities/Debug.hpp"
#include <unordered_map>
#include "../defines.hpp"
#include <vector>

#include "defaultConfig.hpp"

enum ELayouts {
    LAYOUT_DWINDLE = 0,
    LAYOUT_MASTER
};

struct SConfigValue {
    int64_t intValue = -1;
    float floatValue = -1;
    std::string strValue = "";
};

struct SWindowRule {
    std::string szRule;
    std::string szValue;
};

namespace ConfigManager {
    inline std::unordered_map<std::string, SConfigValue> configValues;
    inline time_t       lastModifyTime = 0;

    inline bool         loadBar = false;

    inline std::string  currentCategory = "";

    inline bool         isFirstLaunch = false;

    inline std::string  parseError = ""; // For storing a parse error to display later

    inline std::vector<SWindowRule> windowRules;

    void                init();
    void                loadConfigLoadVars();
    void                tick();

    void                applyKeybindsToX();

    std::vector<SWindowRule> getMatchingRules(xcb_window_t);

    int                 getInt(std::string);
    float               getFloat(std::string);
    std::string         getString(std::string);
};