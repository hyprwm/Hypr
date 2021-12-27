#include "ConfigManager.hpp"
#include "../windowManager.hpp"
#include "../KeybindManager.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

void ConfigManager::init() {
    configValues["border_size"].intValue = 1;
    configValues["gaps_in"].intValue = 5;
    configValues["gaps_out"].intValue = 20;
    configValues["rounding"].intValue = 5;
    configValues["main_mod"].strValue = "SUPER";
    configValues["intelligent_transients"].intValue = 1;

    configValues["focus_when_hover"].intValue = 1;

    configValues["layout"].intValue = LAYOUT_DWINDLE;

    configValues["max_fps"].intValue = 60;

    configValues["bar:monitor"].intValue = 0;
    configValues["bar:enabled"].intValue = 1;
    configValues["bar:height"].intValue = 15;
    configValues["bar:col.bg"].intValue = 0xFF111111;
    configValues["bar:col.high"].intValue = 0xFFFF3333;
    configValues["bar:font.main"].strValue = "Noto Sans";
    configValues["bar:font.secondary"].strValue = "Noto Sans";
    configValues["bar:mod_pad_in"].intValue = 4;
    configValues["bar:no_tray_saving"].intValue = 1;

    configValues["status_command"].strValue = "date +%I:%M\\ %p"; // Time // Deprecated

    // Set Colors ARGB
    configValues["col.active_border"].intValue = 0x77FF3333;
    configValues["col.inactive_border"].intValue = 0x77222222;

    // animations
    configValues["anim:speed"].floatValue = 1;
    configValues["anim:enabled"].intValue = 0;
    configValues["anim:cheap"].intValue = 1;
    configValues["anim:borders"].intValue = 1;
    configValues["anim:workspaces"].intValue = 0;

    if (!g_pWindowManager->statusBar) {
        isFirstLaunch = true;
    }

    lastModifyTime = 0;

    tick();
    applyKeybindsToX();
}

void configSetValueSafe(const std::string& COMMAND, const std::string& VALUE) {
    if (ConfigManager::configValues.find(COMMAND) == ConfigManager::configValues.end()) {
        ConfigManager::parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">: No such field.";
        return;
    }

    auto& CONFIGENTRY = ConfigManager::configValues.at(COMMAND);
    if (CONFIGENTRY.intValue != -1) {
        try {
            if (VALUE.find("0x") == 0) {
                // Values with 0x are hex
                const auto VALUEWITHOUTHEX = VALUE.substr(2);
                CONFIGENTRY.intValue = stol(VALUEWITHOUTHEX, nullptr, 16);
            } else
                CONFIGENTRY.intValue = stol(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
            ConfigManager::parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.floatValue != -1) {
        try {
            CONFIGENTRY.floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
            ConfigManager::parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    } else if (CONFIGENTRY.strValue != "") {
        try {
            CONFIGENTRY.strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
            ConfigManager::parseError = "Error setting value <" + VALUE + "> for field <" + COMMAND + ">.";
        }
    }
}

void handleBind(const std::string& command, const std::string& value) {

    // example:
    // bind=SUPER,G,exec,dmenu_run <args>

    auto valueCopy = value;

    const auto MOD = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto KEY = KeybindManager::getKeyCodeFromName(valueCopy.substr(0, valueCopy.find_first_of(",")));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto HANDLER = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COMMAND = valueCopy;

    Dispatcher dispatcher = nullptr;
    if (HANDLER == "exec") dispatcher = KeybindManager::call;
    if (HANDLER == "killactive") dispatcher = KeybindManager::killactive;
    if (HANDLER == "fullscreen") dispatcher = KeybindManager::toggleActiveWindowFullscreen;
    if (HANDLER == "movewindow") dispatcher = KeybindManager::movewindow;
    if (HANDLER == "movefocus") dispatcher = KeybindManager::movefocus;
    if (HANDLER == "movetoworkspace") dispatcher = KeybindManager::movetoworkspace;
    if (HANDLER == "workspace") dispatcher = KeybindManager::changeworkspace;
    if (HANDLER == "togglefloating") dispatcher = KeybindManager::toggleActiveWindowFloating;

    if (dispatcher && KEY != 0)
        KeybindManager::keybinds.push_back(Keybind(KeybindManager::modToMask(MOD), KEY, COMMAND, dispatcher));
}

void handleRawExec(const std::string& command, const std::string& args) {
    // Exec in the background dont wait for it.
    RETURNIFBAR;

    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
}

void handleStatusCommand(const std::string& command, const std::string& args) {
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->setStatusCommand(args);
}

void parseModule(const std::string& COMMANDC, const std::string& VALUE) {
    SBarModule module;

    auto valueCopy = VALUE;

    const auto ALIGN = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    if (ALIGN == "pad") {
        const auto ALIGNR = valueCopy.substr(0, valueCopy.find_first_of(","));
        valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

        const auto PADW = valueCopy;

        if (ALIGNR == "left") module.alignment = LEFT;
        else if (ALIGNR == "right") module.alignment = RIGHT;
        else if (ALIGNR == "center") module.alignment = CENTER;

        try {
            module.pad = stol(PADW);
        } catch (...) {
            Debug::log(ERR, "Module creation pad error: invalid pad");
            ConfigManager::parseError = "Module creation error in pad: invalid pad.";
            return;
        }

        module.isPad = true;

        module.color = 0;
        module.bgcolor = 0;

        g_pWindowManager->statusBar->modules.push_back(module);

        return;
    }

    const auto ICON = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COL1 = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COL2 = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto UPDATE = valueCopy.substr(0, valueCopy.find_first_of(","));
    valueCopy = valueCopy.substr(valueCopy.find_first_of(",") + 1);

    const auto COMMAND = valueCopy;

    if (ALIGN == "left") module.alignment = LEFT;
    else if (ALIGN == "right") module.alignment = RIGHT;
    else if (ALIGN == "center") module.alignment = CENTER;

    try {
        module.color = stol(COL1.substr(2), nullptr, 16);
        module.bgcolor = stol(COL2.substr(2), nullptr, 16);
    } catch (...) {
        Debug::log(ERR, "Module creation color error: invalid color");
        ConfigManager::parseError = "Module creation error in color: invalid color.";
        return;
    }

    try {
        module.updateEveryMs = stol(UPDATE);
    } catch (...) {
        Debug::log(ERR, "Module creation error: invalid update interval");
        ConfigManager::parseError = "Module creation error in interval: invalid interval.";
        return;
    }

    module.icon = ICON;

    module.value = COMMAND;

    g_pWindowManager->statusBar->modules.push_back(module);
}

void parseBarLine(const std::string& line) {

    // And parse
    // check if command

    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;


    const auto COMMAND = line.substr(0, EQUALSPLACE);
    const auto VALUE = line.substr(EQUALSPLACE + 1);

    // Now check commands
    if (COMMAND == "module") {
        if (g_pWindowManager->statusBar)
            parseModule(COMMAND, VALUE);
    } else {
        // We need to parse those to let the main thread know e.g. the bar height
        configSetValueSafe("bar:" + COMMAND, VALUE);
    }
}

void parseAnimLine(const std::string& line) {
    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = line.substr(0, EQUALSPLACE);
    const auto VALUE = line.substr(EQUALSPLACE + 1);

    configSetValueSafe("anim:" + COMMAND, VALUE);
}

void handleWindowRule(const std::string& command, const std::string& value) {
    const auto RULE = value.substr(0, value.find_first_of(","));
    const auto VALUE = value.substr(value.find_first_of(",") + 1);

    // check rule and value
    if (RULE == "" || VALUE == "") {
        return;
    }

    // verify we support a rule
    if (RULE != "float" 
        && RULE != "tile"
        && RULE.find("move") != 0
        && RULE.find("size") != 0
        && RULE.find("monitor") != 0) {
            Debug::log(ERR, "Invalid rule found: " + RULE);
            ConfigManager::parseError = "Invalid rule found: " + RULE;
            return;
        }

    ConfigManager::windowRules.push_back({RULE, VALUE});
}

void parseLine(std::string& line) {
    // first check if its not a comment
    const auto COMMENTSTART = line.find_first_of('#');
    if (COMMENTSTART == 0)
        return;

    // now, cut the comment off
    if (COMMENTSTART != std::string::npos)
        line = line.substr(0, COMMENTSTART);

    // remove shit at the beginning
    while (line[0] == ' ' || line[0] == '\t') {
        line = line.substr(1);
    }

    if (line.find("Bar {") != std::string::npos) {
        ConfigManager::currentCategory = "bar";
        return;
    }

    if (line.find("Animations {") != std::string::npos) {
        ConfigManager::currentCategory = "anim";
        return;
    }

    if (line.find("}") != std::string::npos && ConfigManager::currentCategory != "") {
        ConfigManager::currentCategory = "";
        return;
    }

    if (ConfigManager::currentCategory == "bar") {
        parseBarLine(line);
        return;
    }

    if (ConfigManager::currentCategory == "anim") {
        parseAnimLine(line);
        return;
    }

    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = line.substr(0, EQUALSPLACE);
    const auto VALUE = line.substr(EQUALSPLACE + 1);

    if (COMMAND == "bind") {
        handleBind(COMMAND, VALUE);
        return;
    } else if (COMMAND == "exec") {
        handleRawExec(COMMAND, VALUE);
        return;
    } else if (COMMAND == "exec-once") {
	if (ConfigManager::isFirstLaunch)
        	handleRawExec(COMMAND, VALUE);
        return;
    } else if (COMMAND == "status_command") {
        handleStatusCommand(COMMAND, VALUE);
        return;
    } else if (COMMAND == "windowrule") {
        handleWindowRule(COMMAND, VALUE);
        return;
    }

    configSetValueSafe(COMMAND, VALUE);
}

void ConfigManager::loadConfigLoadVars() {
    const auto ORIGBORDERSIZE = configValues["border_size"].intValue;
    Debug::log(LOG, "Reloading the config!");
    ConfigManager::parseError = ""; // reset the error
    ConfigManager::currentCategory = ""; // reset the category
    ConfigManager::windowRules.clear(); // Clear rules

    if (loadBar && g_pWindowManager->statusBar) {
        // clear modules as we overwrite them
        for (auto& m : g_pWindowManager->statusBar->modules) {
            g_pWindowManager->statusBar->destroyModule(&m);
        }
        g_pWindowManager->statusBar->modules.clear();
    }

    KeybindManager::keybinds.clear();

    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprd.conf" : (std::string) "/.config/hypr/hypr.conf");

    std::ifstream ifs;
    ifs.open(CONFIGPATH.c_str());

    if (!ifs.good()) {
        Debug::log(WARN, "Config reading error. (No file?)");
        ConfigManager::parseError = "The config could not be read. (No file?)";

        ifs.close();
        return;
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                parseLine(line);
            } catch(...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, line);
                
                parseError = "Config error at line " + std::to_string(linenum) + ": Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + ": " + parseError;
            }

            ++linenum;
            
        }

        ifs.close();
    }

    // recalc all workspaces
    g_pWindowManager->recalcAllWorkspaces();

    // Reload the bar as well, don't load it before the default is loaded.
    if (loadBar && g_pWindowManager->statusBar && (configValues["bar:enabled"].intValue == 1 || parseError != "")) {
        g_pWindowManager->statusBar->destroy();

        // make the bar height visible
        if (parseError != "") {
            configValues["bar:height"].intValue = 15;
        }

        if (configValues["bar:monitor"].intValue > g_pWindowManager->monitors.size()) {
            configValues["bar:monitor"].intValue = 0;
            Debug::log(ERR, "Incorrect value in MonitorID for the bar. Setting to 0.");
        }

        g_pWindowManager->statusBar->setup(configValues["bar:monitor"].intValue);
    } else if (g_pWindowManager->statusBar) {
        g_pWindowManager->statusBar->destroy();
    }

    // Ensure correct layout
    if (configValues["layout"].intValue < 0 || configValues["layout"].intValue > 1) {
        Debug::log(ERR, "Invalid layout ID, falling back to 0.");
        configValues["layout"].intValue = 0;
    }

    loadBar = true;
    isFirstLaunch = false;

    if (ORIGBORDERSIZE != configValues["border_size"].intValue) EWMH::refreshAllExtents(); 
}

void ConfigManager::applyKeybindsToX() {
    if (g_pWindowManager->statusBar) {
        Debug::log(LOG, "Not applying the keybinds because status bar not null");
        return;  // If we are in the status bar don't do this.
    }

    Debug::log(LOG, "Applying " + std::to_string(KeybindManager::keybinds.size()) + " keybinds to X.");

    xcb_ungrab_key(g_pWindowManager->DisplayConnection, XCB_GRAB_ANY, g_pWindowManager->Screen->root, XCB_MOD_MASK_ANY);
    xcb_ungrab_button(g_pWindowManager->DisplayConnection, XCB_GRAB_ANY, g_pWindowManager->Screen->root, XCB_MOD_MASK_ANY);

    for (auto& keybind : KeybindManager::keybinds) {
        xcb_grab_key(g_pWindowManager->DisplayConnection, 1, g_pWindowManager->Screen->root,
                     keybind.getMod(), KeybindManager::getKeycodeFromKeysym(keybind.getKeysym()),
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(g_pWindowManager->DisplayConnection);

    // MOD + mouse
    xcb_grab_button(g_pWindowManager->DisplayConnection, 0,
                    g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_pWindowManager->Screen->root, XCB_NONE,
                    1, KeybindManager::modToMask(configValues["main_mod"].strValue));

    xcb_grab_button(g_pWindowManager->DisplayConnection, 0,
                    g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_pWindowManager->Screen->root, XCB_NONE,
                    3, KeybindManager::modToMask(configValues["main_mod"].strValue));

    xcb_flush(g_pWindowManager->DisplayConnection);
}

void ConfigManager::tick() {
    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (ISDEBUG ? (std::string) "/.config/hypr/hyprd.conf" : (std::string) "/.config/hypr/hypr.conf");

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config, error " + std::to_string(errno));
    }

    // check if we need to reload cfg
    if(fileStat.st_mtime != lastModifyTime) {
        lastModifyTime = fileStat.st_mtime;

        ConfigManager::loadConfigLoadVars();

        // So that X updates our grabbed keys.
        ConfigManager::applyKeybindsToX();

        // so that the WM reloads the windows.
        emptyEvent();
    }
}

int ConfigManager::getInt(std::string v) {
    return configValues[v].intValue;
}

float ConfigManager::getFloat(std::string v) {
    return configValues[v].floatValue;
}

std::string ConfigManager::getString(std::string v) {
    return configValues[v].strValue;
}

std::vector<SWindowRule> ConfigManager::getMatchingRules(xcb_window_t w) {
    const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(w);

    if (!PWINDOW)
        return std::vector<SWindowRule>();

    std::vector<SWindowRule> returns;

    for (auto& rule : ConfigManager::windowRules) {
        // check if we have a matching rule
        if (rule.szValue.find("class:") == 0) {
            std::regex classCheck(rule.szValue.substr(strlen("class:")));

            if (!std::regex_search(PWINDOW->getClassName(), classCheck))
                continue;
        } else if (rule.szValue.find("role:") == 0) {
            std::regex roleCheck(rule.szValue.substr(strlen("role:")));

            if (!std::regex_search(PWINDOW->getRoleName(), roleCheck))
                continue;
        } else {
            continue;
        }

        // applies. Read the rule and behave accordingly
        Debug::log(LOG, "Window rule " + rule.szRule + "," + rule.szValue + " matched.");

        returns.push_back(rule);
    }

    return returns;
}