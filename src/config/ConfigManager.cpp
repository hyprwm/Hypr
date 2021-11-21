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
    configValues["bar_height"].intValue = 15;
    configValues["max_fps"].intValue = 60;

    ConfigManager::loadConfigLoadVars();
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

    Dispatcher dispatcher = nullptr;
    if (HANDLER == "exec") dispatcher = KeybindManager::call;
    if (HANDLER == "killactive") dispatcher = KeybindManager::killactive;
    if (HANDLER == "fullscreen") dispatcher = KeybindManager::toggleActiveWindowFullscreen;
    if (HANDLER == "movewindow") dispatcher = KeybindManager::call;
    if (HANDLER == "workspace") dispatcher = KeybindManager::changeworkspace;

    if (dispatcher)
        KeybindManager::keybinds.push_back(Keybind(mod, KEY, COMMAND, dispatcher));
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
    }

    if (ConfigManager::configValues.find(COMMAND) == ConfigManager::configValues.end()) 
        return;

    auto& CONFIGENTRY = ConfigManager::configValues.at(COMMAND);
    if (CONFIGENTRY.intValue != -1) {
        try {
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
    }

    ifs.close();

    g_pWindowManager->setAllWindowsDirty();

    // Reload the bar as well
    g_pWindowManager->statusBar.destroy();
    g_pWindowManager->statusBar.setup(Vector2D(-1,-1), Vector2D(g_pWindowManager->Screen->width_in_pixels, configValues["bar_height"].intValue));
}

void emptyEvent() {
    xcb_expose_event_t exposeEvent;
    exposeEvent.window = g_pWindowManager->statusBar.getWindowID();
    exposeEvent.response_type = 0;
    exposeEvent.x = 0;
    exposeEvent.y = 0;
    exposeEvent.width = g_pWindowManager->Screen->width_in_pixels;
    exposeEvent.height = g_pWindowManager->Screen->height_in_pixels;
    xcb_send_event(g_pWindowManager->DisplayConnection, false, g_pWindowManager->Screen->root, XCB_EVENT_MASK_EXPOSURE, (char*)&exposeEvent);
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