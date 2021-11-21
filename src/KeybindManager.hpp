#pragma once

#include "utilities/Keybind.hpp"
#include <vector>
#include "windowManager.hpp"

namespace KeybindManager {
    inline std::vector<Keybind> keybinds;

    unsigned int        modToMask(MODS);

    Keybind*            findKeybindByKey(int mod, xcb_keysym_t keysym);
    void                reloadAllKeybinds();
    xcb_keysym_t        getKeysymFromKeycode(xcb_keycode_t keycode);
    xcb_keycode_t       getKeycodeFromKeysym(xcb_keysym_t keysym);

    // Dispatchers
    void                call(std::string args);
    void                killactive(std::string args);
    void                movewindow(std::string args);
    void                changeworkspace(std::string args);
    void                toggleActiveWindowFullscreen(std::string args);
};