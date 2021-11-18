#include "KeybindManager.hpp"

Keybind* KeybindManager::findKeybindByKey(int mod, xcb_keysym_t keysym) {
    for(auto& key : KeybindManager::keybinds) {
        if (keysym == key.getKeysym() && mod == key.getMod()) {
            return &key;
        }
    }
    return nullptr;
}