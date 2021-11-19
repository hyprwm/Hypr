#include "KeybindManager.hpp"

#include <algorithm>

Keybind* KeybindManager::findKeybindByKey(int mod, xcb_keysym_t keysym) {
    for(auto& key : KeybindManager::keybinds) {
        if (keysym == key.getKeysym() && mod == key.getMod()) {
            return &key;
        }
    }
    return nullptr;
}

void KeybindManager::reloadAllKeybinds() {
    KeybindManager::keybinds.clear();

    // todo: config
    KeybindManager::keybinds.push_back(Keybind(MOD_SUPER, 0x72 /* R */, "krunner", &KeybindManager::call));
    KeybindManager::keybinds.push_back(Keybind(MOD_SUPER, 0x62 /* G */, "google-chrome-stable", &KeybindManager::call));
}

unsigned int KeybindManager::modToMask(MODS mod) {
    switch(mod) {
        case MOD_NONE:
            return 0;
        case MOD_SUPER:
            return XCB_MOD_MASK_4;
        case MOD_SHIFT:
            return XCB_MOD_MASK_SHIFT;
    }

    return 0;
}

xcb_keysym_t KeybindManager::getKeysymFromKeycode(xcb_keycode_t keycode) {
    const auto KEYSYMS = xcb_key_symbols_alloc(WindowManager::DisplayConnection);
    const auto KEYSYM = (!(KEYSYMS) ? 0 : xcb_key_symbols_get_keysym(KEYSYMS, keycode, 0));
    xcb_key_symbols_free(KEYSYMS);
    return KEYSYM;
}

xcb_keycode_t KeybindManager::getKeycodeFromKeysym(xcb_keysym_t keysym) {
    const auto KEYSYMS = xcb_key_symbols_alloc(WindowManager::DisplayConnection);
    const auto KEYCODE = (!(KEYSYMS) ? NULL : xcb_key_symbols_get_keycode(KEYSYMS, keysym));
    xcb_key_symbols_free(KEYSYMS);
    return KEYCODE ? *KEYCODE : 0;
}

// Dispatchers

void KeybindManager::call(std::string args) {
    if (fork() == 0) {
        setsid();
        if (fork() != 0) {
            _exit(0);
        }

        // fix the args
        std::string command = args.substr(0, args.find_first_of(" "));

        int ARGNO = std::count(args.begin(), args.end(), ' ');
        if(ARGNO > 0)
            ARGNO -= 1;

        if(ARGNO) {
            char* argsarr[ARGNO];

            for (int i = 0; i < ARGNO; ++i) {
                args = args.substr(args.find_first_of(' ') + 1);
                argsarr[i] = (char*)args.substr(0, args.find_first_of(' ')).c_str();
            }

            Debug::log(LOG, "Executing " + command + " with " + std::to_string(ARGNO) + " args:");
            for (int i = 0; i < ARGNO; ++i) {
                Debug::log(NONE, argsarr[i]);
            }

            execvp((char*)command.c_str(), (char**)argsarr);
        } else {
            char* argsarr[1];
            argsarr[0] = "";

            Debug::log(LOG, "Executing " + command + " with 0 args.");

            execvp((char*)command.c_str(), (char**)argsarr);
        }

        
        _exit(0);
    }
    wait(NULL);
}