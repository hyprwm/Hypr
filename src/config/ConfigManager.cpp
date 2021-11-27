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

    configValues["max_fps"].intValue = 60;

    configValues["bar_monitor"].intValue = 0;
    configValues["bar_enabled"].intValue = 1;
    configValues["bar_height"].intValue = 15;

    configValues["status_command"].strValue = "date +%I:%M\\ %p"; // Time


    // Set Colors ARGB
    configValues["col.active_border"].intValue = 0x77FF3333;
    configValues["col.inactive_border"].intValue = 0x77222222;


    // animations
    configValues["anim.speed"].floatValue = 1;
    configValues["anim.enabled"].intValue = 1;

    loadConfigLoadVars();
    applyKeybindsToX();
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

    MODS mod = MOD_NONE;

    if (MOD == "SUPER") mod = MOD_SUPER;
    else if (MOD == "SHIFT") mod = MOD_SHIFT;
    else if (MOD == "SUPERSHIFT" || MOD == "SHIFTSUPER") mod = MOD_SHIFTSUPER;
    else if (MOD == "SUPERCTRL" || MOD == "CTRLSUPER") mod = MOD_CTRLSUPER;
    else if (MOD == "CTRL") mod = MOD_CTRL;
    else if (MOD == "CTRLSHIFT" || MOD == "SHIFTCTRL") mod = MOD_SHIFTCTRL;

    Dispatcher dispatcher = nullptr;
    if (HANDLER == "exec") dispatcher = KeybindManager::call;
    if (HANDLER == "killactive") dispatcher = KeybindManager::killactive;
    if (HANDLER == "fullscreen") dispatcher = KeybindManager::toggleActiveWindowFullscreen;
    if (HANDLER == "movewindow") dispatcher = KeybindManager::movewindow;
    if (HANDLER == "movetoworkspace") dispatcher = KeybindManager::movetoworkspace;
    if (HANDLER == "workspace") dispatcher = KeybindManager::changeworkspace;
    if (HANDLER == "togglefloating") dispatcher = KeybindManager::toggleActiveWindowFloating;

    if (dispatcher)
        KeybindManager::keybinds.push_back(Keybind(mod, KEY, COMMAND, dispatcher));
}

void handleRawExec(const std::string& command, const std::string& args) {
    exec(args.c_str());
}

void handleStatusCommand(const std::string& command, const std::string& args) {
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->setStatusCommand(args);
}

void parseLine(std::string& line) {
    // first check if its not a comment
    const auto COMMENTSTART = line.find_first_of('#');
    if (COMMENTSTART == 0)
        return;

    // now, cut the comment off
    if (COMMENTSTART != std::string::npos)
        line = line.substr(COMMENTSTART);

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
    } else if (COMMAND == "status_command") {
        handleStatusCommand(COMMAND, VALUE);
        return;
    }

    if (ConfigManager::configValues.find(COMMAND) == ConfigManager::configValues.end()) 
        return;

    auto& CONFIGENTRY = ConfigManager::configValues.at(COMMAND);
    if (CONFIGENTRY.intValue != -1) {
        try {
            if (VALUE.find("0x") == 0) {
                // Values with 0x are hex
                const auto VALUEWITHOUTHEX = VALUE.substr(2);
                CONFIGENTRY.intValue = stoi(VALUEWITHOUTHEX, nullptr, 16);
            } else
                CONFIGENTRY.intValue = stoi(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
        }
    } else if (CONFIGENTRY.floatValue != -1) {
        try {
            CONFIGENTRY.floatValue = stof(VALUE);
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
        }
    } else if (CONFIGENTRY.strValue != "") {
        try {
            CONFIGENTRY.strValue = VALUE;
        } catch (...) {
            Debug::log(WARN, "Error reading value of " + COMMAND);
        }
    }
}

void ConfigManager::loadConfigLoadVars() {
    Debug::log(LOG, "Reloading the config!");

    KeybindManager::keybinds.clear();

    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (std::string) "/.config/hypr/hypr.conf";

    std::ifstream ifs;
    ifs.open(CONFIGPATH.c_str());

    if (!ifs.good()) {
        Debug::log(WARN, "Config reading error. (No file?)");
        return;
    }

    std::string line = "";
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                parseLine(line);
            } catch(...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, line);
            }
            
        }

        ifs.close();
    }

    g_pWindowManager->setAllWindowsDirty();

    // Reload the bar as well, don't load it before the default is loaded.
    if (loadBar && g_pWindowManager->statusBar) {
        g_pWindowManager->statusBar->destroy();
        g_pWindowManager->statusBar->setup(configValues["bar_monitor"].intValue);
    }

    loadBar = true;
}

void ConfigManager::applyKeybindsToX() {
    if (g_pWindowManager->statusBar) {
        Debug::log(LOG, "Not applying the keybinds because status bar not null");
        return;  // If we are in the status bar don't do this.
    }
        

    xcb_ungrab_key(g_pWindowManager->DisplayConnection, XCB_GRAB_ANY, g_pWindowManager->Screen->root, XCB_MOD_MASK_ANY);

    for (auto& keybind : KeybindManager::keybinds) {
        xcb_grab_key(g_pWindowManager->DisplayConnection, 1, g_pWindowManager->Screen->root,
                     KeybindManager::modToMask(keybind.getMod()), KeybindManager::getKeycodeFromKeysym(keybind.getKeysym()),
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(g_pWindowManager->DisplayConnection);

    // MOD + mouse
    xcb_grab_button(g_pWindowManager->DisplayConnection, 0,
                    g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_pWindowManager->Screen->root, XCB_NONE,
                    1, KeybindManager::modToMask(MOD_SUPER));

    xcb_grab_button(g_pWindowManager->DisplayConnection, 0,
                    g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_pWindowManager->Screen->root, XCB_NONE,
                    3, KeybindManager::modToMask(MOD_SUPER));

    xcb_flush(g_pWindowManager->DisplayConnection);
}

void ConfigManager::tick() {
    const char* const ENVHOME = getenv("HOME");

    const std::string CONFIGPATH = ENVHOME + (std::string)"/.config/hypr/hypr.conf";

    struct stat fileStat;
    int err = stat(CONFIGPATH.c_str(), &fileStat);
    if (err != 0) {
        Debug::log(WARN, "Error at ticking config, error" + std::to_string(errno));
    }

    // check if we need to reload cfg
    if(fileStat.st_mtime > lastModifyTime) {
        lastModifyTime = fileStat.st_mtime;

        ConfigManager::loadConfigLoadVars();

        // So that X updates our grabbed keys.
        ConfigManager::applyKeybindsToX();

        // so that the WM reloads the windows.
        emptyEvent();
    }
}

int ConfigManager::getInt(std::string_view v) {
    return configValues[v].intValue;
}

float ConfigManager::getFloat(std::string_view v) {
    return configValues[v].floatValue;
}

std::string ConfigManager::getString(std::string_view v) {
    return configValues[v].strValue;
}