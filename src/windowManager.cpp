#include "windowManager.hpp"
#include "./events/events.hpp"
#include <string.h>

xcb_visualtype_t* CWindowManager::setupColors() {
    auto depthIter = xcb_screen_allowed_depths_iterator(Screen);
    xcb_visualtype_iterator_t visualIter;
    for (; depthIter.rem; xcb_depth_next(&depthIter)) {
        if (depthIter.data->depth == Depth) {
            visualIter = xcb_depth_visuals_iterator(depthIter.data);
            return visualIter.data;
        }
    }

    return nullptr;
}

void CWindowManager::setupRandrMonitors() {
    auto ScreenResReply = xcb_randr_get_screen_resources_current_reply(DisplayConnection, xcb_randr_get_screen_resources_current(DisplayConnection, Screen->root), NULL);
    if (!ScreenResReply) {
        Debug::log(ERR, "ScreenResReply NULL!");
        return;
    }

    Debug::log(LOG, "Setting up RandR!");

    const auto MONITORNUM = xcb_randr_get_screen_resources_current_outputs_length(ScreenResReply);
    auto OUTPUTS = xcb_randr_get_screen_resources_current_outputs(ScreenResReply);

    xcb_randr_get_output_info_reply_t* outputReply;
    xcb_randr_get_crtc_info_reply_t* crtcReply;

    Debug::log(LOG, "Monitors found: " + std::to_string(MONITORNUM));

    for (int i = 0; i < MONITORNUM; i++) {
        outputReply = xcb_randr_get_output_info_reply(DisplayConnection, xcb_randr_get_output_info(DisplayConnection, OUTPUTS[i], XCB_CURRENT_TIME), NULL);
        if (!outputReply || outputReply->crtc == XCB_NONE)
            continue;
        crtcReply = xcb_randr_get_crtc_info_reply(DisplayConnection, xcb_randr_get_crtc_info(DisplayConnection, outputReply->crtc, XCB_CURRENT_TIME), NULL);
        if (!crtcReply)
            continue;

        monitors.push_back(SMonitor());

        monitors[monitors.size() - 1].vecPosition = Vector2D(crtcReply->x, crtcReply->y);
        monitors[monitors.size() - 1].vecSize = Vector2D(crtcReply->width == 0 ? 1920 : crtcReply->width, crtcReply->height);

        monitors[monitors.size() - 1].ID = monitors.size() - 1;

        char* name = (char*)xcb_randr_get_output_info_name(outputReply);
        int nameLen = xcb_randr_get_output_info_name_length(outputReply);

        for (int j = 0; j < nameLen; ++j) {
            monitors[monitors.size() - 1].szName += name[j];
        }

        Debug::log(NONE, "Monitor " + monitors[monitors.size() - 1].szName + ": " + std::to_string(monitors[i].vecSize.x) + "x" + std::to_string(monitors[monitors.size() - 1].vecSize.y) +
            ", at " + std::to_string(monitors[monitors.size() - 1].vecPosition.x) + "," + std::to_string(monitors[monitors.size() - 1].vecSize.y));
    }

    const auto EXTENSIONREPLY = xcb_get_extension_data(DisplayConnection, &xcb_randr_id);
    if (!EXTENSIONREPLY->present)
        Debug::log(ERR, "RandR extension missing");
    else {
        //listen for screen change events
        xcb_randr_select_input(DisplayConnection, Screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    }

    // Sort monitors for my convenience thanks
    std::sort(monitors.begin(), monitors.end(), [](SMonitor& a, SMonitor& b) {
        return a.vecPosition.x < b.vecPosition.x;
    });
}

void CWindowManager::setupManager() {
    setupRandrMonitors();

    ConfigManager::init();

    if (monitors.size() == 0) {
        // RandR failed!
        Debug::log(WARN, "RandR failed!");

        monitors.push_back(SMonitor());
        monitors[0].vecPosition = Vector2D(0, 0);
        monitors[0].vecSize = Vector2D(Screen->width_in_pixels, Screen->height_in_pixels);
        monitors[0].ID = 0;
        monitors[0].szName = "Screen";
    }

    Debug::log(LOG, "RandR done.");

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

    Debug::log(LOG, "Keys done.");

    // Add workspaces to the monitors
    for (int i = 0; i < monitors.size(); ++i) {
        CWorkspace protoWorkspace;
        protoWorkspace.setID(i + 1);
        protoWorkspace.setMonitor(i);
        protoWorkspace.setHasFullscreenWindow(false);
        workspaces.push_back(protoWorkspace);
        activeWorkspaces.push_back(workspaces[i].getID());
    }

    Debug::log(LOG, "Workspace protos done.");
    //

    // init visual type, default 32 bit depth
    // TODO: fix this, ugh
    Depth = 24; //32
    VisualType = setupColors();
    if (VisualType == NULL) {
        Depth = 24;
        VisualType = setupColors();
    }

    // ---- INIT THE BAR ---- //

    for (auto& monitor : monitors) {
        if (monitor.primary) {
            statusBar.setup(ConfigManager::configValues["bar_monitor"].intValue);
        }
    }

    // Update bar info
    updateBarInfo();

    // start its' update thread
    Events::setThread();

    Debug::log(LOG, "Bar done.");

    ConfigManager::loadConfigLoadVars();

    Debug::log(LOG, "Finished setup!");
}

bool CWindowManager::handleEvent() {
    if (xcb_connection_has_error(DisplayConnection))
        return false;

    xcb_flush(DisplayConnection);
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
            case XCB_BUTTON_PRESS:
                Events::eventKeyPress(ev);
                Debug::log(LOG, "Event dispatched BUTTON_PRESS");
                break;
            case XCB_EXPOSE:
                Events::eventExpose(ev);
                Debug::log(LOG, "Event dispatched EXPOSE");
                break;
            case XCB_KEY_PRESS:
                Events::eventButtonPress(ev);
                Debug::log(LOG, "Event dispatched KEY_PRESS");
                break;

            default:
                //Debug::log(WARN, "Unknown event: " + std::to_string(ev->response_type & ~0x80));
                break;
        }

        free(ev);
    }

    // refresh and apply the parameters of all dirty windows.
    refreshDirtyWindows();

    // remove unused workspaces
    cleanupUnusedWorkspaces();

    xcb_flush(DisplayConnection);

    return true;
}

// TODO: add an empty space check
void CWindowManager::performSanityCheckForWorkspace(int WorkspaceID) {
    for (auto& windowA : windows) {
        if (windowA.getWorkspaceID() != WorkspaceID)
            continue;

        for (auto& windowB : windows) {
            if (windowB.getWorkspaceID() != WorkspaceID)
                continue;

            if (windowB.getDrawable() == windowA.getDrawable())
                continue;

            // Check if A and B overlap, if don't, continue
            if ((windowA.getPosition().x >= (windowB.getPosition() + windowB.getSize()).x || windowB.getPosition().x >= (windowA.getPosition() + windowA.getSize()).x
                || windowA.getPosition().y >= (windowB.getPosition() + windowB.getSize()).y || windowB.getPosition().y >= (windowA.getPosition() + windowA.getSize()).y)) {
                    continue;
            }

            // Overlap detected! Fix window B
            if (windowB.getIsFloating()) {
                calculateNewTileSetOldTile(&windowB);
            } else {
                calculateNewFloatingWindow(&windowB);
            }
        }
    }
}

void CWindowManager::cleanupUnusedWorkspaces() {
    std::vector<CWorkspace> temp = workspaces;

    workspaces.clear();

    for (auto& work : temp) {
        if (!isWorkspaceVisible(work.getID())) {
            // check if it has any children
            bool hasChildren = false;
            for (auto& window : windows) {
                if (window.getWorkspaceID() == work.getID()) {
                    hasChildren = true;
                    break;
                }
            }

            if (hasChildren) {
                // Has windows opened on it.
                workspaces.push_back(work);
            }
        } else {
            // Foreground workspace
            workspaces.push_back(work);
        }
    }

    // Update bar info
    updateBarInfo();
}

void CWindowManager::refreshDirtyWindows() {
    for(auto& window : windows) {
        if (window.getDirty()) {

            setEffectiveSizePosUsingConfig(&window);

            // Fullscreen flag
            bool bHasFullscreenWindow = getWorkspaceByID(window.getWorkspaceID())->getHasFullscreenWindow();

            // first and foremost, let's check if the window isn't on a hidden workspace
            // or that it is not a non-fullscreen window in a fullscreen workspace
            if (!isWorkspaceVisible(window.getWorkspaceID())
                || (bHasFullscreenWindow && !window.getFullscreen())) {
                // Move it to hades
                Values[0] = (int)1500000; // hmu when monitors actually have that many pixels
                Values[1] = (int)1500000; // and we are still using xorg =)
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

                // Set the size JIC.
                Values[0] = (int)window.getEffectiveSize().x;
                Values[1] = (int)window.getEffectiveSize().y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

                continue;
            }

            // Fullscreen window. No border, all screen.
            if (window.getFullscreen()) {
                Values[0] = 0;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                Values[0] = 0x555555;  // GRAY :)
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);

                const auto MONITOR = getMonitorFromWindow(&window);

                Values[0] = (int)MONITOR->vecSize.x;
                Values[1] = (int)MONITOR->vecSize.y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

                Values[0] = (int)MONITOR->vecPosition.x;
                Values[1] = (int)MONITOR->vecPosition.y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

                continue;
            }

            Values[0] = (int)window.getEffectiveSize().x;
            Values[1] = (int)window.getEffectiveSize().y;
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

            // Update the position because the border makes the window jump
            // I have added the bordersize vec2d before in the setEffectiveSizePosUsingConfig function.
            Values[0] = (int)window.getEffectivePosition().x - ConfigManager::getInt("border_size");
            Values[1] = (int)window.getEffectivePosition().y - ConfigManager::getInt("border_size");
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

            // Focused special border.
            if (window.getDrawable() == LastWindow) {
                Values[0] = (int)ConfigManager::getInt("border_size");
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                Values[0] = 0xFF3333;  // RED :)
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            } else {

                Values[0] = 0x222222;  // GRAY :)
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
        Values[0] = 0x555555;  // GRAY :)
        xcb_change_window_attributes(DisplayConnection, LastWindow, XCB_CW_BORDER_PIXEL, Values);

        Values[0] = 0xFF3333;  // RED :)
        xcb_change_window_attributes(DisplayConnection, window, XCB_CW_BORDER_PIXEL, Values);

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

    const auto MONITOR = getMonitorFromWindow(pWindow);

    // set some flags.
    const bool DISPLAYLEFT          = STICKS(pWindow->getPosition().x, MONITOR->vecPosition.x);
    const bool DISPLAYRIGHT         = STICKS(pWindow->getPosition().x + pWindow->getSize().x, MONITOR->vecPosition.x + MONITOR->vecSize.x);
    const bool DISPLAYTOP           = STICKS(pWindow->getPosition().y, MONITOR->vecPosition.y);
    const bool DISPLAYBOTTOM        = STICKS(pWindow->getPosition().y + pWindow->getSize().y, MONITOR->vecPosition.y + MONITOR->vecSize.y);

    pWindow->setEffectivePosition(pWindow->getPosition() + Vector2D(ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size")));
    pWindow->setEffectiveSize(pWindow->getSize() - (Vector2D(ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size")) * 2));

    //TODO: make windows with no bar taller, this aint working chief

    // do gaps, set top left
    pWindow->setEffectivePosition(pWindow->getEffectivePosition() + Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID != statusBar.getMonitorID() ? ConfigManager::getInt("bar_height") : 0) : ConfigManager::getInt("gaps_in")));
    // fix to old size bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID != statusBar.getMonitorID() ? ConfigManager::getInt("bar_height") : 0) : ConfigManager::getInt("gaps_in")));
    // set bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYRIGHT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYBOTTOM ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in")));
}

CWindow* CWindowManager::findWindowAtCursor() {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    xcb_query_pointer_reply_t* pointerreply = xcb_query_pointer_reply(DisplayConnection, POINTERCOOKIE, NULL);
    if (!pointerreply) {
        Debug::log(ERR, "Couldn't query pointer.");
        return nullptr;
    }

    Vector2D cursorPos = Vector2D(pointerreply->root_x, pointerreply->root_y);

    free(pointerreply);

    const auto WORKSPACE = activeWorkspaces[getMonitorFromCursor()->ID];

    for (auto& window : windows) {
        if (window.getWorkspaceID() == WORKSPACE && !window.getIsFloating()) {

            if (cursorPos.x >= window.getPosition().x 
                && cursorPos.x <= window.getPosition().x + window.getSize().x
                && cursorPos.y >= window.getPosition().y
                && cursorPos.y <= window.getPosition().y + window.getSize().y) {

                return &window;
            }
        }
    }

    return nullptr;
}

void CWindowManager::calculateNewTileSetOldTile(CWindow* pWindow) {
    auto PLASTWINDOW = getWindowFromDrawable(LastWindow);

    if (PLASTWINDOW && (PLASTWINDOW->getIsFloating() || PLASTWINDOW->getWorkspaceID() != pWindow->getWorkspaceID())) {
        // find a window manually
        PLASTWINDOW = findWindowAtCursor();
    }

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
        const auto MONITOR = getMonitorFromCursor();
        pWindow->setSize(Vector2D(MONITOR->vecSize.x, MONITOR->vecSize.y));
        pWindow->setPosition(Vector2D(MONITOR->vecPosition.x, MONITOR->vecPosition.y));
    }
}

void CWindowManager::calculateNewFloatingWindow(CWindow* pWindow) {
    if (!pWindow)
        return;

    pWindow->setPosition(pWindow->getDefaultPosition());
    pWindow->setSize(pWindow->getDefaultSize());
}

void CWindowManager::calculateNewWindowParams(CWindow* pWindow) {
    // And set old one's if needed.
    if (!pWindow)
        return;

    if (!pWindow->getIsFloating()) {
        calculateNewTileSetOldTile(pWindow);
    } else {
        calculateNewFloatingWindow(pWindow);
    }

    pWindow->setDirty(true);
}

bool CWindowManager::isNeighbor(CWindow* a, CWindow* b) {

    if (a->getWorkspaceID() != b->getWorkspaceID())
        return false; // Different workspaces

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
        if (w.getDrawable() == a->getDrawable() || w.getDrawable() == toEat->getDrawable() || w.getWorkspaceID() != toEat->getWorkspaceID()
            || w.getIsFloating() || getMonitorFromWindow(&w) != getMonitorFromWindow(toEat))
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

    const auto WORKSPACE = activeWorkspaces[getMonitorFromWindow(pClosedWindow)->ID];

    // Fix if was fullscreen
    if (pClosedWindow->getFullscreen())
        g_pWindowManager->getWorkspaceByID(WORKSPACE)->setHasFullscreenWindow(false);

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
}

CWindow* CWindowManager::getNeighborInDir(char dir) {

    const auto CURRENTWINDOW = getWindowFromDrawable(LastWindow);

    if (!CURRENTWINDOW)
        return nullptr;

    const auto POSA = CURRENTWINDOW->getPosition();
    const auto SIZEA = CURRENTWINDOW->getSize();

    for (auto& w : windows) {
        if (w.getDrawable() == CURRENTWINDOW->getDrawable() || w.getWorkspaceID() != CURRENTWINDOW->getWorkspaceID())
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

    // finish by moving the cursor to the current window
    warpCursorTo(CURRENTWINDOW->getPosition() + CURRENTWINDOW->getSize() / 2.f);
}

void CWindowManager::changeWorkspaceByID(int ID) {

    const auto MONITOR = getMonitorFromCursor();

    // mark old workspace dirty
    setAllWorkspaceWindowsDirtyByID(activeWorkspaces[MONITOR->ID]);

    for (auto& workspace : workspaces) {
        if (workspace.getID() == ID) {
            activeWorkspaces[workspace.getMonitor()] = workspace.getID();
            LastWindow = -1;

            // Perform a sanity check for the new workspace
            performSanityCheckForWorkspace(ID);

            // Update bar info
            updateBarInfo();

            // mark new as dirty
            setAllWorkspaceWindowsDirtyByID(activeWorkspaces[MONITOR->ID]);

            return;
        }
    }

    // If we are here it means the workspace is new. Let's create it.
    CWorkspace newWorkspace;
    newWorkspace.setID(ID);
    newWorkspace.setMonitor(MONITOR->ID);
    workspaces.push_back(newWorkspace);
    activeWorkspaces[MONITOR->ID] = workspaces[workspaces.size() - 1].getID();
    LastWindow = -1;

    // Perform a sanity check for the new workspace
    performSanityCheckForWorkspace(ID);

    // Update bar info
    updateBarInfo();

    // no need for the new dirty, it's empty
}

void CWindowManager::setAllWindowsDirty() {
    for (auto& window : windows) {
        window.setDirty(true);
    }
}

void CWindowManager::setAllWorkspaceWindowsDirtyByID(int ID) {
    int workspaceID = -1;
    for (auto& workspace : workspaces) {
        if (workspace.getID() == ID) {
            workspaceID = workspace.getID();
            break;
        }
    }

    if (workspaceID == -1)
        return;

    for (auto& window : windows) {
        if (window.getWorkspaceID() == workspaceID)
            window.setDirty(true);
    }
}

int CWindowManager::getHighestWorkspaceID() {
    int max = -1;
    for (auto& workspace : workspaces) {
        if (workspace.getID() > max) {
            max = workspace.getID();
        }
    }

    return max;
}

CWorkspace* CWindowManager::getWorkspaceByID(int ID) {
    for (auto& workspace : workspaces) {
        if (workspace.getID() == ID) {
            return &workspace;
        }
    }

    return nullptr;
}

SMonitor* CWindowManager::getMonitorFromWindow(CWindow* pWindow) {
    return &monitors[pWindow->getMonitor()];
}

SMonitor* CWindowManager::getMonitorFromCursor() {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    xcb_query_pointer_reply_t* pointerreply = xcb_query_pointer_reply(DisplayConnection, POINTERCOOKIE, NULL);
    if (!pointerreply) {
        Debug::log(ERR, "Couldn't query pointer.");
        return nullptr;
    }

    const auto CURSORPOS = Vector2D(pointerreply->root_x, pointerreply->root_y);
    free(pointerreply);

    for (auto& monitor : monitors) {
        if (VECINRECT(CURSORPOS, monitor.vecPosition.x, monitor.vecPosition.y, monitor.vecPosition.x + monitor.vecSize.x, monitor.vecPosition.y + monitor.vecSize.y))
            return &monitor;
    }

    // should never happen tho, I'm using >= and the cursor cant get outside the screens, i hope.
    return nullptr;
}

bool CWindowManager::isWorkspaceVisible(int workspaceID) {

    for (auto& workspace : activeWorkspaces) {
        if (workspace == workspaceID)
            return true;
    }

    return false;
}

void CWindowManager::updateBarInfo() {
    statusBar.openWorkspaces.clear();
    for (auto& workspace : workspaces) {
        statusBar.openWorkspaces.push_back(workspace.getID());
    }

    std::sort(statusBar.openWorkspaces.begin(), statusBar.openWorkspaces.end());

    statusBar.setCurrentWorkspace(activeWorkspaces[getMonitorFromCursor()->ID]);
}