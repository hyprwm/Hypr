#include "KeybindManager.hpp"
#include "utilities/Util.hpp"
#include "events/events.hpp"

#include <algorithm>
#include <string.h>

Keybind* KeybindManager::findKeybindByKey(int mod, xcb_keysym_t keysym) {
    for(auto& key : KeybindManager::keybinds) {
        if (keysym == key.getKeysym() && mod == key.getMod()) {
            return &key;
        }
    }
    return nullptr;
}

uint32_t KeybindManager::getKeyCodeFromName(std::string name) {
    if (name == "")
        return 0;

    transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name.length() == 1) {
        // key
        std::string command = "xmodmap -pk | grep \"(" + name + ")\"";
        std::string returnValue = exec(command.c_str());

        try {
            returnValue = returnValue.substr(returnValue.find_first_of('x') + 1);
            returnValue = returnValue.substr(0, returnValue.find_first_of(' '));

            return std::stoi(returnValue, nullptr, 16); // return hex to int
        } catch(...) { }
    } else {
        if (name == "return" || name == "enter") {
            return 0xff0d;
        } else if (name == "left") {
            return 0xff51;
        } else if (name == "right") {
            return 0xff53;
        } else if (name == "up") {
            return 0xff52;
        } else if (name == "down") {
            return 0xff54;
        } else if (name == "space") {
            return 0x20;
        }
    }

    return 0;
}

unsigned int KeybindManager::modToMask(MODS mod) {
    switch(mod) {
        case MOD_NONE:
            return 0;
        case MOD_SUPER:
            return XCB_MOD_MASK_4;
        case MOD_SHIFT:
            return XCB_MOD_MASK_SHIFT;
        case MOD_SHIFTSUPER:
            return XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT;
        case MOD_SHIFTCTRL:
            return XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL;
        case MOD_CTRL:
            return XCB_MOD_MASK_CONTROL;
        case MOD_CTRLSUPER:
            return XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_4;
    }

    return 0;
}

xcb_keysym_t KeybindManager::getKeysymFromKeycode(xcb_keycode_t keycode) {
    const auto KEYSYMS = xcb_key_symbols_alloc(g_pWindowManager->DisplayConnection);
    const auto KEYSYM = (!(KEYSYMS) ? 0 : xcb_key_symbols_get_keysym(KEYSYMS, keycode, 0));
    xcb_key_symbols_free(KEYSYMS);
    return KEYSYM;
}

xcb_keycode_t KeybindManager::getKeycodeFromKeysym(xcb_keysym_t keysym) {
    const auto KEYSYMS = xcb_key_symbols_alloc(g_pWindowManager->DisplayConnection);
    const auto KEYCODE = (!(KEYSYMS) ? NULL : xcb_key_symbols_get_keycode(KEYSYMS, keysym));
    xcb_key_symbols_free(KEYSYMS);
    return KEYCODE ? *KEYCODE : 0;
}

// Dispatchers

void KeybindManager::killactive(std::string args) {
    // args unused
    xcb_destroy_window(g_pWindowManager->DisplayConnection, g_pWindowManager->LastWindow);
}

void KeybindManager::call(std::string args) {

    if (fork() == 0) {
        //setsid();

        execl("/bin/sh", "/bin/sh", "-c", args.c_str(), nullptr);

        _exit(0);
    }
    //wait(NULL);
}

void KeybindManager::movewindow(std::string arg) {
    g_pWindowManager->moveActiveWindowTo(arg[0]);
}

void KeybindManager::movetoworkspace(std::string arg) {
    try {
        g_pWindowManager->moveActiveWindowToWorkspace(stoi(arg));
    } catch (...) {
        Debug::log(ERR, "Invalid arg in movetoworkspace, arg: " + arg);
    }
    
}

void KeybindManager::changeworkspace(std::string arg) {
    int ID = -1;
    try {
        ID = std::stoi(arg.c_str());
    } catch (...) { ; }

    if (ID != -1) {
        Debug::log(LOG, "Changing the current workspace to " + std::to_string(ID));

        g_pWindowManager->changeWorkspaceByID(ID);
    }
}

void KeybindManager::toggleActiveWindowFullscreen(std::string unusedArg) {
    const auto MONITOR = g_pWindowManager->getMonitorFromWindow(g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow));

    g_pWindowManager->setAllWorkspaceWindowsDirtyByID(g_pWindowManager->activeWorkspaces[MONITOR->ID]);

    if (auto WINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow) ; WINDOW) {
        WINDOW->setFullscreen(!WINDOW->getFullscreen());
        g_pWindowManager->getWorkspaceByID(g_pWindowManager->activeWorkspaces[MONITOR->ID])->setHasFullscreenWindow(WINDOW->getFullscreen());
    }
}

void KeybindManager::toggleActiveWindowFloating(std::string unusedArg) {
    if (const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); PWINDOW) {
        PWINDOW->setIsFloating(!PWINDOW->getIsFloating());
        PWINDOW->setDirty(true);

        // Fix window as if it's closed if we just made it floating
        if (PWINDOW->getIsFloating()) {
            g_pWindowManager->fixWindowOnClose(PWINDOW);
            g_pWindowManager->calculateNewWindowParams(PWINDOW);
        }
        else {
            // It's remapped again

            // SAVE ALL INFO NOW, THE POINTER WILL BE DEAD
            const auto RESTOREACSIZE = PWINDOW->getDefaultSize();
            const auto RESTOREACPOS = PWINDOW->getDefaultPosition();
            const auto RESTOREWINID = PWINDOW->getDrawable();

            g_pWindowManager->removeWindowFromVectorSafe(PWINDOW->getDrawable());
            const auto PNEWWINDOW = Events::remapWindow(RESTOREWINID, true);

            PNEWWINDOW->setDefaultPosition(RESTOREACPOS);
            PNEWWINDOW->setDefaultSize(RESTOREACSIZE);
        }
        
    }
}