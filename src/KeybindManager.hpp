#pragma once

#include "utilities/Keybind.hpp"
#include <vector>
#include "windowManager.hpp"

#include <map>

namespace KeybindManager {
    inline std::vector<Keybind> keybinds;

    unsigned int        modToMask(std::string);

    Keybind*            findKeybindByKey(int mod, xcb_keysym_t keysym);
    xcb_keysym_t        getKeysymFromKeycode(xcb_keycode_t keycode);
    xcb_keycode_t       getKeycodeFromKeysym(xcb_keysym_t keysym);

    uint32_t            getKeyCodeFromName(std::string);

    // Dispatchers
    void                call(std::string args);
    void                killactive(std::string args);
    void                movewindow(std::string args);
    void                movefocus(std::string args);
    void                changeworkspace(std::string args);
    void                changetolastworkspace(std::string args);
    void                toggleActiveWindowFullscreen(std::string args);
    void                toggleActiveWindowFloating(std::string args);
    void                movetoworkspace(std::string args);
    void                changeSplitRatio(std::string args);
    void                togglePseudoActive(std::string args);
    void                toggleScratchpad(std::string args);
    void                nextWorkspace(std::string args);
    void                lastWorkspace(std::string args);
    void                pinActive(std::string args);
};
