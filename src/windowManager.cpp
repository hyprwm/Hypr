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

    // TODO: this stopped working on my machine for some reason.
    // i3 works though...?

    // finds 0 monitors

    XCBQUERYCHECK(RANDRVER, xcb_randr_query_version_reply(
        DisplayConnection, xcb_randr_query_version(DisplayConnection, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION), &errorRANDRVER), "RandR query failed!" );

        
    free(RANDRVER);

    Debug::log(LOG, "Setting up RandR! Query: v1.5.");

    XCBQUERYCHECK(MONITORS, xcb_randr_get_monitors_reply(DisplayConnection, xcb_randr_get_monitors(DisplayConnection, Screen->root, true), &errorMONITORS), "Couldn't get monitors. " + std::to_string(errorMONITORS->error_code));

    const auto MONITORNUM = xcb_randr_get_monitors_monitors_length(MONITORS);

    Debug::log(LOG, "Found " + std::to_string(MONITORNUM) + " Monitors!");

    if (MONITORNUM < 1) {
        // TODO: RandR 1.4 maybe for people with ancient hardware?
        Debug::log(ERR, "RandR returned an invalid amount of monitors. Falling back to 1 monitor.");
        return;
    }

    for (xcb_randr_monitor_info_iterator_t iterator = xcb_randr_get_monitors_monitors_iterator(MONITORS); iterator.rem; xcb_randr_monitor_info_next(&iterator)) {
        const auto MONITORINFO = iterator.data;

        // basically an XCBQUERYCHECK but with continue; as its not fatal
        xcb_generic_error_t* error;
        const auto ATOMNAME = xcb_get_atom_name_reply(DisplayConnection, xcb_get_atom_name(DisplayConnection, MONITORINFO->name), &error);
        if (error != NULL) {
            Debug::log(ERR, "Failed to get monitor info...");
            free(error);
            free(ATOMNAME);
            continue;
        }
        free(error);

        monitors.push_back(SMonitor());

        const auto NAMELEN = xcb_get_atom_name_name_length(ATOMNAME);
        const auto NAME = xcb_get_atom_name_name(ATOMNAME);

        free(ATOMNAME);

        for (int j = 0; j < NAMELEN; ++j) {
            monitors[monitors.size() - 1].szName += NAME[j];
        }

        monitors[monitors.size() - 1].vecPosition = Vector2D(MONITORINFO->x, MONITORINFO->y);
        monitors[monitors.size() - 1].vecSize = Vector2D(MONITORINFO->width, MONITORINFO->height);

        monitors[monitors.size() - 1].primary = MONITORINFO->primary;

        monitors[monitors.size() - 1].ID = monitors.size() - 1;

        Debug::log(NONE, "Monitor " + monitors[monitors.size() - 1].szName + ": " + std::to_string(monitors[monitors.size() - 1].vecSize.x) + "x" + std::to_string(monitors[monitors.size() - 1].vecSize.y) +
                             ", at " + std::to_string(monitors[monitors.size() - 1].vecPosition.x) + "," + std::to_string(monitors[monitors.size() - 1].vecPosition.y) + ", ID: " + std::to_string(monitors[monitors.size() - 1].ID));
    }

    free(MONITORS);

    /*

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
                             ", at " + std::to_string(monitors[monitors.size() - 1].vecPosition.x) + "," + std::to_string(monitors[monitors.size() - 1].vecPosition.y) + ", ID: " + std::to_string(monitors[monitors.size() - 1].ID));
    } */

    const auto EXTENSIONREPLY = xcb_get_extension_data(DisplayConnection, &xcb_randr_id);
    if (!EXTENSIONREPLY->present)
        Debug::log(ERR, "RandR extension missing");
    else {
        //listen for screen change events
        xcb_randr_select_input(DisplayConnection, Screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    }

    xcb_flush(DisplayConnection);
}

void CWindowManager::setupManager() {
    setupRandrMonitors();

    if (monitors.size() == 0) {
        // RandR failed!
        Debug::log(WARN, "RandR failed!");

        #define TESTING_MON_AMOUNT 3
        for (int i = 0; i < TESTING_MON_AMOUNT /* Testing on 3 monitors, RandR shouldnt fail on a real desktop */; ++i) {
            monitors.push_back(SMonitor());
            monitors[i].vecPosition = Vector2D(i * Screen->width_in_pixels / TESTING_MON_AMOUNT, 0);
            monitors[i].vecSize = Vector2D(Screen->width_in_pixels / TESTING_MON_AMOUNT, Screen->height_in_pixels);
            monitors[i].ID = i;
            monitors[i].szName = "Screen" + std::to_string(i);
        }
    }

    Debug::log(LOG, "RandR done.");

    Values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(DisplayConnection, Screen->root,
                                         XCB_CW_EVENT_MASK, Values);

    ConfigManager::init();

    Debug::log(LOG, "Keys done.");

    // Add workspaces to the monitors
    for (long unsigned int i = 0; i < monitors.size(); ++i) {
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
        while (animationUtilBusy) {
            ; // wait for it to finish
        }

        // Set thread state, halt animations until done.
        mainThreadBusy = true;

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
                Events::eventButtonPress(ev);
                Debug::log(LOG, "Event dispatched BUTTON_PRESS");
                break;
            case XCB_BUTTON_RELEASE:
                Events::eventButtonRelease(ev);
                Debug::log(LOG, "Event dispatched BUTTON_RELEASE");
                break;
            case XCB_MOTION_NOTIFY:
                Events::eventMotionNotify(ev);
                //Debug::log(LOG, "Event dispatched MOTION_NOTIFY"); // Spam!!
                break;
            case XCB_EXPOSE:
                Events::eventExpose(ev);
                Debug::log(LOG, "Event dispatched EXPOSE");
                break;
            case XCB_KEY_PRESS:
                Events::eventKeyPress(ev);
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

    // Restore thread state
    mainThreadBusy = false;

    return true;
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
            window.setDirty(false);

            // Check if the window isn't a node
            if (window.getChildNodeAID() != 0) 
                continue;
                
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

            Values[0] = (int)window.getRealSize().x;
            Values[1] = (int)window.getRealSize().y;
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

            // Update the position because the border makes the window jump
            // I have added the bordersize vec2d before in the setEffectiveSizePosUsingConfig function.
            Values[0] = (int)window.getRealPosition().x - ConfigManager::getInt("border_size");
            Values[1] = (int)window.getRealPosition().y - ConfigManager::getInt("border_size");
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

            // Focused special border.
            if (window.getDrawable() == LastWindow) {
                Values[0] = (int)ConfigManager::getInt("border_size");
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                Values[0] = ConfigManager::getInt("col.active_border");
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            } else {
                Values[0] = ConfigManager::getInt("col.inactive_border");
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            }

            Debug::log(LOG, "Refreshed dirty window, with an ID of " + std::to_string(window.getDrawable()));
        }
    }
}

void CWindowManager::setFocusedWindow(xcb_drawable_t window) {
    if (window && window != Screen->root) {
        xcb_set_input_focus(DisplayConnection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);

        // Fix border from the old window that was in focus.
        Values[0] = ConfigManager::getInt("col.inactive_border");
        xcb_change_window_attributes(DisplayConnection, LastWindow, XCB_CW_BORDER_PIXEL, Values);

        Values[0] = ConfigManager::getInt("col.active_border");
        xcb_change_window_attributes(DisplayConnection, window, XCB_CW_BORDER_PIXEL, Values);

        float values[1];
        if (g_pWindowManager->getWindowFromDrawable(window) && g_pWindowManager->getWindowFromDrawable(window)->getIsFloating()) {
            values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(g_pWindowManager->DisplayConnection, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
        }

        LastWindow = window;
    }
}

CWindow* CWindowManager::getWindowFromDrawable(int64_t window) {
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

void CWindowManager::removeWindowFromVectorSafe(int64_t window) {

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

    // do gaps, set top left
    pWindow->setEffectivePosition(pWindow->getEffectivePosition() + Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID == statusBar.getMonitorID() ? ConfigManager::getInt("bar_height") : 0) : ConfigManager::getInt("gaps_in")));
    // fix to old size bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID == statusBar.getMonitorID() ? ConfigManager::getInt("bar_height") : 0) : ConfigManager::getInt("gaps_in")));
    // set bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYRIGHT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYBOTTOM ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in")));
}

CWindow* CWindowManager::findWindowAtCursor() {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    Vector2D cursorPos = getCursorPos();

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
    
    // Get the parent and both children, one of which will be pWindow
    const auto PPARENT = getWindowFromDrawable(pWindow->getParentNodeID());

    if (!PPARENT) {
        // New window on this workspace.
        // Open a fullscreen window.
        const auto MONITOR = getMonitorFromCursor();
        if (!MONITOR) {
            Debug::log(ERR, "Monitor was nullptr! (calculateNewTileSetOldTile)");
            return;
        }
            
        pWindow->setSize(Vector2D(MONITOR->vecSize.x, MONITOR->vecSize.y));
        pWindow->setPosition(Vector2D(MONITOR->vecPosition.x, MONITOR->vecPosition.y));

        return;
    }

    // Get the sibling
    const auto PSIBLING = getWindowFromDrawable(PPARENT->getChildNodeAID() == pWindow->getDrawable() ? PPARENT->getChildNodeBID() : PPARENT->getChildNodeAID());

    // Should NEVER be null
    if (PSIBLING) {
        const auto PLASTSIZE = PPARENT->getSize();
        const auto PLASTPOS = PPARENT->getPosition();

        if (PLASTSIZE.x > PLASTSIZE.y) {
            PSIBLING->setPosition(Vector2D(PLASTPOS.x, PLASTPOS.y));
            PSIBLING->setSize(Vector2D(PLASTSIZE.x / 2.f, PLASTSIZE.y));
            pWindow->setSize(Vector2D(PLASTSIZE.x / 2.f, PLASTSIZE.y));
            pWindow->setPosition(Vector2D(PLASTPOS.x + PLASTSIZE.x / 2.f, PLASTPOS.y));
        } else {
            PSIBLING->setPosition(Vector2D(PLASTPOS.x, PLASTPOS.y));
            PSIBLING->setSize(Vector2D(PLASTSIZE.x, PLASTSIZE.y / 2.f));
            pWindow->setSize(Vector2D(PLASTSIZE.x, PLASTSIZE.y / 2.f));
            pWindow->setPosition(Vector2D(PLASTPOS.x, PLASTPOS.y + PLASTSIZE.y / 2.f));
        }

        PSIBLING->setDirty(true);
    } else {
        Debug::log(ERR, "Sibling node was null?? pWindow x,y,w,h: " + std::to_string(pWindow->getPosition().x) + " "
                            + std::to_string(pWindow->getPosition().y) + " " + std::to_string(pWindow->getSize().x) + " "
                            + std::to_string(pWindow->getSize().y));
    }

    Values[0] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(DisplayConnection, pWindow->getDrawable(), XCB_CONFIG_WINDOW_STACK_MODE, Values);
}

void CWindowManager::calculateNewFloatingWindow(CWindow* pWindow) {
    if (!pWindow)
        return;

    pWindow->setPosition(pWindow->getDefaultPosition());
    pWindow->setSize(pWindow->getDefaultSize());

    Values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(DisplayConnection, pWindow->getDrawable(), XCB_CONFIG_WINDOW_STACK_MODE, Values);
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

    setEffectiveSizePosUsingConfig(pWindow);

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

    // Get the parent and both children, one of which will be pWindow
    const auto PPARENT = getWindowFromDrawable(pClosedWindow->getParentNodeID());

    if (!PPARENT) 
        return; // if there was no parent, we do not need to update anything. it was a fullscreen window, the only one on a given workspace.

    // Get the sibling
    const auto PSIBLING = getWindowFromDrawable(PPARENT->getChildNodeAID() == pClosedWindow->getDrawable() ? PPARENT->getChildNodeBID() : PPARENT->getChildNodeAID());

    if (!PSIBLING) {
        Debug::log(ERR, "No sibling found in fixOnClose! (Corrupted tree...?)");
        return;
    }

    PSIBLING->setPosition(PPARENT->getPosition());
    PSIBLING->setSize(PPARENT->getSize());

    // make the sibling replace the parent
    PSIBLING->setParentNodeID(PPARENT->getParentNodeID());

    if (PPARENT->getParentNodeID() != 0 
        && getWindowFromDrawable(PPARENT->getParentNodeID())) {
            if (getWindowFromDrawable(PPARENT->getParentNodeID())->getChildNodeAID() == PPARENT->getDrawable()) {
                getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeAID(PSIBLING->getDrawable());
            } else {
                getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeBID(PSIBLING->getDrawable());
            }
    }

    // Make the sibling eat the closed window
    PSIBLING->setDirtyRecursive(true);
    PSIBLING->recalcSizePosRecursive();

    // Remove the parent
    removeWindowFromVectorSafe(PPARENT->getDrawable());

    if (findWindowAtCursor())
        setFocusedWindow(findWindowAtCursor()->getDrawable());  // Set focus. :)
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
        free(pointerreply);
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

    if (!MONITOR) {
        Debug::log(ERR, "Monitor was nullptr! (changeWorkspaceByID)");
        return;
    }

    // mark old workspace dirty
    setAllWorkspaceWindowsDirtyByID(activeWorkspaces[MONITOR->ID]);

    for (auto& workspace : workspaces) {
        if (workspace.getID() == ID) {
            activeWorkspaces[workspace.getMonitor()] = workspace.getID();
            LastWindow = -1;

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
    const auto CURSORPOS = getCursorPos();

    for (auto& monitor : monitors) {
        if (VECINRECT(CURSORPOS, monitor.vecPosition.x, monitor.vecPosition.y, monitor.vecPosition.x + monitor.vecSize.x, monitor.vecPosition.y + monitor.vecSize.y))
            return &monitor;
    }

    // should never happen tho, I'm using >= and the cursor cant get outside the screens, i hope.
    return nullptr;
}

Vector2D CWindowManager::getCursorPos() {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    xcb_query_pointer_reply_t* pointerreply = xcb_query_pointer_reply(DisplayConnection, POINTERCOOKIE, NULL);
    if (!pointerreply) {
        Debug::log(ERR, "Couldn't query pointer.");
        free(pointerreply);
        return Vector2D(0,0);
    }

    const auto CURSORPOS = Vector2D(pointerreply->root_x, pointerreply->root_y);
    free(pointerreply);

    return CURSORPOS;
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

    if (!getMonitorFromCursor()) {
        Debug::log(ERR, "Monitor was null! (updateBarInfo)");
        return;
    }

    statusBar.setCurrentWorkspace(activeWorkspaces[getMonitorFromCursor()->ID]);
}

void CWindowManager::setAllFloatingWindowsTop() {
    for (auto& window : windows) {
        if (window.getIsFloating()) {
            Values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(g_pWindowManager->DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_STACK_MODE, Values);
        }
    }
}