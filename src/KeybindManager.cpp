#include "KeybindManager.hpp"
#include "utilities/Util.hpp"
#include "events/events.hpp"
#include "windowManager.hpp"

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

    const auto ORIGINALCASENAME = name;
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
        } else {
            // unknown key, find in the xmodmap
            std::string command = "xmodmap -pk | grep \"(" + ORIGINALCASENAME + ")\"";
            std::string returnValue = exec(command.c_str());

            try {
                returnValue = returnValue.substr(returnValue.find_first_of('x') + 1);
                returnValue = returnValue.substr(0, returnValue.find_first_of(' '));

                return std::stoi(returnValue, nullptr, 16);  // return hex to int
            } catch (...) {
                Debug::log(ERR, "Unknown key: " + ORIGINALCASENAME);
            }
        }
    }

    return 0;
}

unsigned int KeybindManager::modToMask(std::string mod) {

    unsigned int sum = 0;

    if (CONTAINS(mod, "SUPER") || CONTAINS(mod, "MOD4"))    sum |= XCB_MOD_MASK_4;
    if (CONTAINS(mod, "SHIFT"))                             sum |= XCB_MOD_MASK_SHIFT;
    if (CONTAINS(mod, "CTRL"))                              sum |= XCB_MOD_MASK_CONTROL;
    if (CONTAINS(mod, "ALT") || CONTAINS(mod, "MOD1"))      sum |= XCB_MOD_MASK_1;

    return sum;
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
    const auto PLASTWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow);

    if (!PLASTWINDOW)
        return;
    
    if (PLASTWINDOW->getCanKill()) {
        // Send a kill message
        xcb_client_message_event_t event;
        bzero(&event, sizeof(event));
        event.response_type = XCB_CLIENT_MESSAGE;
        event.window = PLASTWINDOW->getDrawable();
        event.type = HYPRATOMS["WM_PROTOCOLS"];
        event.format = 32;
        event.data.data32[0] = HYPRATOMS["WM_DELETE_WINDOW"];
        event.data.data32[1] = 0;

        xcb_send_event(g_pWindowManager->DisplayConnection, 0, PLASTWINDOW->getDrawable(), XCB_EVENT_MASK_NO_EVENT, (const char*)&event);

        return; // Do not remove it yet. The user might've cancelled the operation or something. We will get
                // an unmap event.
    } else 
        xcb_kill_client(g_pWindowManager->DisplayConnection, g_pWindowManager->LastWindow);

    g_pWindowManager->closeWindowAllChecks(g_pWindowManager->LastWindow);
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

void KeybindManager::movefocus(std::string arg) {
    g_pWindowManager->moveActiveFocusTo(arg[0]);
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

void KeybindManager::changetolastworkspace(std::string arg) {
    Debug::log(LOG, "Changing the current workspace to the last workspace");
    g_pWindowManager->changeToLastWorkspace();
}

void KeybindManager::toggleActiveWindowFullscreen(std::string unusedArg) {
    g_pWindowManager->toggleWindowFullscrenn(g_pWindowManager->LastWindow);
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
            const auto RESTORECANKILL = PWINDOW->getCanKill();

            g_pWindowManager->removeWindowFromVectorSafe(PWINDOW->getDrawable());

            CWindow newWindow;
            newWindow.setDrawable(RESTOREWINID);
            newWindow.setFirstOpen(false);
            g_pWindowManager->addWindowToVectorSafe(newWindow);

            const auto PNEWWINDOW = Events::remapWindow(RESTOREWINID, true);

            PNEWWINDOW->setDefaultPosition(RESTOREACPOS);
            PNEWWINDOW->setDefaultSize(RESTOREACSIZE);
            PNEWWINDOW->setCanKill(RESTORECANKILL);
        }

        // EWMH to let everyone know
        EWMH::updateClientList();

        EWMH::updateWindow(PWINDOW->getDrawable());

        g_pWindowManager->setAllFloatingWindowsTop();
    }
}

void KeybindManager::changeSplitRatio(std::string args) {
    g_pWindowManager->changeSplitRatioCurrent(args[0]);
}