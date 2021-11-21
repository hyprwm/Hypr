#include "KeybindManager.hpp"
#include "utilities/Util.hpp"

#include <algorithm>

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
    xcb_kill_client(g_pWindowManager->DisplayConnection, g_pWindowManager->LastWindow);
}

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
            Debug::log(LOG, "Executing " + command + " with 0 args.");

            execvp((char*)command.c_str(), nullptr);
        }

        
        _exit(0);
    }
    wait(NULL);
}

void KeybindManager::movewindow(std::string arg) {
    g_pWindowManager->moveActiveWindowTo(arg[0]);
}

void KeybindManager::changeworkspace(std::string arg) {
    int ID = -1;
    try {
        ID = std::stoi(arg.c_str());
    } catch (...) { ; }

    if (ID != -1) {
        Debug::log(LOG, "Changing the current workspace to " + std::to_string(ID));

        //                                                                                vvvv shouldn't be nullptr wallah
        g_pWindowManager->setAllWorkspaceWindowsDirtyByID(g_pWindowManager->activeWorkspace->getID());
        g_pWindowManager->changeWorkspaceByID(ID);
        g_pWindowManager->setAllWorkspaceWindowsDirtyByID(ID);
    }
}

void KeybindManager::toggleActiveWindowFullscreen(std::string unusedArg) {
    g_pWindowManager->setAllWorkspaceWindowsDirtyByID(g_pWindowManager->activeWorkspace->getID());

    if (auto WINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow) ; WINDOW) {
        WINDOW->setFullscreen(!WINDOW->getFullscreen());
        g_pWindowManager->activeWorkspace->setHasFullscreenWindow(WINDOW->getFullscreen());
    }
}

void KeybindManager::toggleActiveWindowFloating(std::string unusedArg) {
    if (const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); PWINDOW) {
        PWINDOW->setIsFloating(!PWINDOW->getIsFloating());
        PWINDOW->setDirty(true);

        // Fix window as if it's closed if we just made it floating
        if (PWINDOW->getIsFloating())
            g_pWindowManager->fixWindowOnClose(PWINDOW);

        g_pWindowManager->calculateNewWindowParams(PWINDOW);
    }
}