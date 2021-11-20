#include "windowManager.hpp"
#include "./events/events.hpp"

void CWindowManager::setupManager() {
    KeybindManager::reloadAllKeybinds();

    Values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(DisplayConnection, Screen->root,
                                         XCB_CW_EVENT_MASK, Values);
    xcb_ungrab_key(DisplayConnection, XCB_GRAB_ANY, Screen->root, XCB_MOD_MASK_ANY);

    for (auto& keybind : KeybindManager::keybinds) {
        xcb_grab_key(DisplayConnection, 1, Screen->root,
            KeybindManager::modToMask(keybind.getMod()), KeybindManager::getKeycodeFromKeysym(keybind.getKeysym()),
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    xcb_flush(DisplayConnection);

    xcb_grab_button(DisplayConnection, 0,
        Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, Screen->root, XCB_NONE,
        1, KeybindManager::modToMask(MOD_SUPER));
    
    xcb_grab_button(DisplayConnection, 0,
        Screen->root, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, Screen->root, XCB_NONE,
        3, KeybindManager::modToMask(MOD_SUPER));
    
    xcb_flush(DisplayConnection);   
}

bool CWindowManager::handleEvent() {
    if (xcb_connection_has_error(DisplayConnection))
        return false;

    const auto ev = xcb_wait_for_event(DisplayConnection);
    if (ev != NULL) {
        switch (ev->response_type & ~0x80) {
            case XCB_ENTER_NOTIFY:
                Events::eventEnter(ev);
                Debug::log(LOG, "Event dispatched ENTER");
                break;
            case XCB_LEAVE_NOTIFY:
                Events::eventLeave(ev);
                Debug::log(LOG, "Event dispatched LEAVE");
                break;
            case XCB_DESTROY_NOTIFY:
                Events::eventDestroy(ev);
                Debug::log(LOG, "Event dispatched DESTROY");
                break;
            case XCB_MAP_REQUEST:
                Events::eventMapWindow(ev);
                Debug::log(LOG, "Event dispatched MAP");
                break;
            case XCB_KEY_PRESS:
            case XCB_BUTTON_PRESS:
                Events::eventKeyPress(ev);
                Debug::log(LOG, "Event dispatched KEYPRESS");
                break;

            default:
                //Debug::log(WARN, "Unknown event: " + std::to_string(ev->response_type & ~0x80));
                break;
        }

        free(ev);
    }

    // refresh and apply the parameters of all dirty windows.
    refreshDirtyWindows();

    xcb_flush(DisplayConnection);

    return true;
}

void CWindowManager::refreshDirtyWindows() {
    for(auto& window : windows) {
        if (window.getDirty()) {
            

            Values[0] = (int)window.getEffectiveSize().x;
            Values[1] = (int)window.getEffectiveSize().y;
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

            Values[0] = (int)window.getEffectivePosition().x;
            Values[1] = (int)window.getEffectivePosition().y;
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

            // Focused special border.
            if (window.getDrawable() == LastWindow) {
                Values[0] = (int)BORDERSIZE;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                // Update the position because the border makes the window jump
                // I have added the bordersize vec2d before in the setEffectiveSizePosUsingConfig function.
                Values[0] = (int)window.getEffectivePosition().x - BORDERSIZE;
                Values[1] = (int)window.getEffectivePosition().y - BORDERSIZE;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

                Values[0] = 0xFF3333;  // RED :)
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            } else {
                Values[0] = 0;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                Values[0] = 0x555555;  // GRAY :)
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            }

            window.setDirty(false);

            Debug::log(LOG, "Refreshed dirty window, with an ID of " + std::to_string(window.getDrawable()));
        }
    }
}

void CWindowManager::setFocusedWindow(xcb_drawable_t window) {
    if (window && window != Screen->root) {
        xcb_set_input_focus(DisplayConnection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);

        // Fix border from the old window that was in focus.
        if (const auto PLASTWINDOW = getWindowFromDrawable(LastWindow); PLASTWINDOW)
            PLASTWINDOW->setDirty(true);

        LastWindow = window;
    }
}

CWindow* CWindowManager::getWindowFromDrawable(xcb_drawable_t window) {
    for(auto& w : windows) {
        if (w.getDrawable() == window) {
            return &w;
        }
    }
    return nullptr;
}

void CWindowManager::addWindowToVectorSafe(CWindow window) {
    for (auto& w : windows) {
        if (w.getDrawable() == window.getDrawable())
            return; // Do not add if already present.
    }
    windows.push_back(window);
}

void CWindowManager::removeWindowFromVectorSafe(xcb_drawable_t window) {

    if (!window)
        return;

    std::vector<CWindow> temp = windows;

    windows.clear();
    
    for(auto p : temp) {
        if (p.getDrawable() != window) {
            windows.push_back(p);
            continue;
        }
    }
}

void CWindowManager::setEffectiveSizePosUsingConfig(CWindow* pWindow) {

    if (!pWindow)
        return;

    // set some flags.
    const bool DISPLAYLEFT          = pWindow->getPosition().x == 0;
    const bool DISPLAYRIGHT         = pWindow->getPosition().x + pWindow->getSize().x == Screen->width_in_pixels;
    const bool DISPLAYTOP           = pWindow->getPosition().y == 0;
    const bool DISPLAYBOTTOM        = pWindow->getPosition().y + pWindow->getSize().y == Screen->height_in_pixels;

    pWindow->setEffectivePosition(pWindow->getPosition() + Vector2D(BORDERSIZE, BORDERSIZE));
    pWindow->setEffectiveSize(pWindow->getSize() - (Vector2D(BORDERSIZE, BORDERSIZE) * 2));

    // do gaps, set top left
    pWindow->setEffectivePosition(pWindow->getEffectivePosition() + Vector2D(DISPLAYLEFT ? GAPS_OUT : GAPS_IN, DISPLAYTOP ? GAPS_OUT : GAPS_IN));
    // fix to old size bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYLEFT ? GAPS_OUT : GAPS_IN, DISPLAYTOP ? GAPS_OUT : GAPS_IN));
    // set bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYRIGHT ? GAPS_OUT : GAPS_IN, DISPLAYBOTTOM ? GAPS_OUT : GAPS_IN));
}

void CWindowManager::calculateNewTileSetOldTile(CWindow* pWindow) {
    const auto PLASTWINDOW = getWindowFromDrawable(LastWindow);
    if (PLASTWINDOW) {
        const auto PLASTSIZE = PLASTWINDOW->getSize();
        const auto PLASTPOS = PLASTWINDOW->getPosition();

        if (PLASTSIZE.x > PLASTSIZE.y) {
            PLASTWINDOW->setSize(Vector2D(PLASTSIZE.x / 2.f, PLASTSIZE.y));
            pWindow->setSize(Vector2D(PLASTSIZE.x / 2.f, PLASTSIZE.y));
            pWindow->setPosition(Vector2D(PLASTPOS.x + PLASTSIZE.x / 2.f, PLASTPOS.y));
        } else {
            PLASTWINDOW->setSize(Vector2D(PLASTSIZE.x, PLASTSIZE.y / 2.f));
            pWindow->setSize(Vector2D(PLASTSIZE.x, PLASTSIZE.y / 2.f));
            pWindow->setPosition(Vector2D(PLASTPOS.x, PLASTPOS.y + PLASTSIZE.y / 2.f));
        }

        PLASTWINDOW->setDirty(true);
    } else {
        // Open a fullscreen window
        pWindow->setSize(Vector2D(Screen->width_in_pixels, Screen->height_in_pixels));
        pWindow->setPosition(Vector2D(0, 0));
    }

    setEffectiveSizePosUsingConfig(pWindow);
    setEffectiveSizePosUsingConfig(PLASTWINDOW);
}

void CWindowManager::calculateNewWindowParams(CWindow* pWindow) {
    // And set old one's if needed.
    if (!pWindow)
        return;

    if (!pWindow->getIsFloating()) {
        calculateNewTileSetOldTile(pWindow);
    }

    pWindow->setDirty(true);
}

bool CWindowManager::isNeighbor(CWindow* a, CWindow* b) {
    const auto POSA = a->getPosition();
    const auto POSB = b->getPosition();
    const auto SIZEA = a->getSize();
    const auto SIZEB = b->getSize();

    if (POSA.x != 0) {
        if (STICKS(POSA.x, (POSB.x + SIZEB.x))) {
            return true;
        }
    }
    if (POSA.y != 0) {
        if (STICKS(POSA.y, (POSB.y + SIZEB.y))) {
            return true;
        }
    }

    if (POSB.x != 0) {
        if (STICKS(POSB.x, (POSA.x + SIZEA.x))) {
            return true;
        }
    }
    if (POSB.y != 0) {
        if (STICKS(POSB.y, (POSA.y + SIZEA.y))) {
            return true;
        }
    }

    return false;
}

bool CWindowManager::canEatWindow(CWindow* a, CWindow* toEat) {
    // Pos is min of both.
    const auto POSAFTEREAT = Vector2D(std::min(a->getPosition().x, toEat->getPosition().x), std::min(a->getPosition().y, toEat->getPosition().y));

    // Size is pos + size max - pos
    const auto OPPCORNERA = Vector2D(POSAFTEREAT) + a->getSize();
    const auto OPPCORNERB = toEat->getPosition() + toEat->getSize();

    const auto SIZEAFTEREAT = Vector2D(std::max(OPPCORNERA.x, OPPCORNERB.x), std::max(OPPCORNERA.y, OPPCORNERB.y)) - POSAFTEREAT;

    const auto doOverlap = [&](CWindow* b) {
        const auto RIGHT1 = Vector2D(POSAFTEREAT.x + SIZEAFTEREAT.x, POSAFTEREAT.y + SIZEAFTEREAT.y);
        const auto RIGHT2 = b->getPosition() + b->getSize();
        const auto LEFT1 = POSAFTEREAT;
        const auto LEFT2 = b->getPosition();

        return !(LEFT1.x >= RIGHT2.x || LEFT2.x >= RIGHT1.x || LEFT1.y >= RIGHT2.y || LEFT2.y >= RIGHT1.y);
    };

    for (auto& w : windows) {
        if (w.getDrawable() == a->getDrawable() || w.getDrawable() == toEat->getDrawable())
            continue;

        if (doOverlap(&w))
            return false;
    }

    return true;
}

void CWindowManager::eatWindow(CWindow* a, CWindow* toEat) {

    // Size is pos + size max - pos
    const auto OPPCORNERA = a->getPosition() + a->getSize();
    const auto OPPCORNERB = toEat->getPosition() + toEat->getSize();

    // Pos is min of both.
    a->setPosition(Vector2D(std::min(a->getPosition().x, toEat->getPosition().x), std::min(a->getPosition().y, toEat->getPosition().y)));

    a->setSize(Vector2D(std::max(OPPCORNERA.x, OPPCORNERB.x), std::max(OPPCORNERA.y, OPPCORNERB.y)) - a->getPosition());
}

void CWindowManager::fixWindowOnClose(CWindow* pClosedWindow) {
    if (!pClosedWindow)
        return;

    // get the first neighboring window
    CWindow* neighbor = nullptr;
    for(auto& w : windows) {
        if (w.getDrawable() == pClosedWindow->getDrawable())
            continue;

        if (isNeighbor(&w, pClosedWindow) && canEatWindow(&w, pClosedWindow)) {
            neighbor = &w;
            break;
        }
    }

    if (!neighbor)
        return; // No neighbor. Don't update, easy.

    // update neighbor to "eat" closed.
    eatWindow(neighbor, pClosedWindow);

    neighbor->setDirty(true);
    setFocusedWindow(neighbor->getDrawable()); // Set focus. :)

    setEffectiveSizePosUsingConfig(neighbor);
}

CWindow* CWindowManager::getNeighborInDir(char dir) {

    const auto CURRENTWINDOW = getWindowFromDrawable(LastWindow);

    if (!CURRENTWINDOW)
        return nullptr;

    const auto POSA = CURRENTWINDOW->getPosition();
    const auto SIZEA = CURRENTWINDOW->getSize();

    for (auto& w : windows) {
        if (w.getDrawable() == CURRENTWINDOW->getDrawable())
            continue;

        const auto POSB = w.getPosition();
        const auto SIZEB = w.getSize();

        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x))
                    return &w;
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x))
                    return &w;
                break;
            case 't':
                if (STICKS(POSA.y, POSB.y + SIZEB.y))
                    return &w;
                break;
            case 'b':
                if (STICKS(POSA.y + SIZEA.y, POSB.y))
                    return &w;
                break;
        }
    }

    return nullptr;
}

// I don't know if this works, it might be an issue with my nested Xorg session I am using rn to test this.
// Will check later.
// TODO:
void CWindowManager::warpCursorTo(Vector2D to) {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    xcb_query_pointer_reply_t* pointerreply = xcb_query_pointer_reply(DisplayConnection, POINTERCOOKIE, NULL);
    if (!pointerreply) {
        Debug::log(ERR, "Couldn't query pointer.");
        return;
    }

    xcb_warp_pointer(DisplayConnection, XCB_NONE, Screen->root, 0, 0, 0, 0, (int)to.x, (int)to.y);
    free(pointerreply);
}

void CWindowManager::moveActiveWindowTo(char dir) {
    const auto CURRENTWINDOW = getWindowFromDrawable(LastWindow);

    if (!CURRENTWINDOW)
        return;

    const auto neighbor = getNeighborInDir(dir);

    if (!neighbor)
        return;

    // swap their stuff and mark dirty
    const auto TEMP_SIZEA = CURRENTWINDOW->getSize();
    const auto TEMP_POSA = CURRENTWINDOW->getPosition();

    CURRENTWINDOW->setSize(neighbor->getSize());
    CURRENTWINDOW->setPosition(neighbor->getPosition());

    neighbor->setSize(TEMP_SIZEA);
    neighbor->setPosition(TEMP_POSA);

    CURRENTWINDOW->setDirty(true);
    neighbor->setDirty(true);

    setEffectiveSizePosUsingConfig(neighbor);
    setEffectiveSizePosUsingConfig(CURRENTWINDOW);

    // finish by moving the cursor to the current window
    warpCursorTo(CURRENTWINDOW->getPosition() + CURRENTWINDOW->getSize() / 2.f);
}