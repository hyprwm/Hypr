#include "windowManager.hpp"
#include "./events/events.hpp"
#include <string.h>

xcb_visualtype_t* CWindowManager::setupColors(const int& desiredDepth) {
    auto depthIter = xcb_screen_allowed_depths_iterator(Screen);
    if (depthIter.data) {
        for (; depthIter.rem; xcb_depth_next(&depthIter)) {
            if (desiredDepth == 0 || desiredDepth == depthIter.data->depth) {
                for (auto it = xcb_depth_visuals_iterator(depthIter.data); it.rem; xcb_visualtype_next(&it)) {
                    return it.data;
                }
            }
        }
        if (desiredDepth > 0) {
            return setupColors(0);
        }
    }
    return nullptr;
}

void CWindowManager::setupDepth() {
    Depth = 24;
    VisualType = setupColors(Depth);
}

void CWindowManager::createAndOpenAllPipes() {
    system("mkdir -p /tmp/hypr");
    system("cat \" \" > /tmp/hypr/hyprbarin");
    system("cat \" \" > /tmp/hypr/hyprbarout");
    system("cat \" \" > /tmp/hypr/hyprbarind");
    system("cat \" \" > /tmp/hypr/hyprbaroutd");
}

void CWindowManager::updateRootCursor() {
    if (xcb_cursor_context_new(DisplayConnection, Screen, &pointerContext) < 0) {
        Debug::log(ERR, "Creating a cursor context failed!");
        return;
    }

    pointerCursor = xcb_cursor_load_cursor(pointerContext, "left_ptr");

    Debug::log(LOG, "Cursor created with ID " + std::to_string(pointerCursor));

    // Set the cursor
    uint32_t values[1] = { pointerCursor };
    xcb_change_window_attributes(DisplayConnection, Screen->root, XCB_CW_CURSOR, values);
}

void CWindowManager::setupColormapAndStuff() {
    VisualType = xcb_aux_find_visual_by_attrs(Screen, -1, 32); // Transparency by default

    Depth = xcb_aux_get_depth_of_visual(Screen, VisualType->visual_id);
    Colormap = xcb_generate_id(DisplayConnection);
    const auto COOKIE = xcb_create_colormap(DisplayConnection, XCB_COLORMAP_ALLOC_NONE, Colormap, Screen->root, VisualType->visual_id);

    const auto XERR = xcb_request_check(DisplayConnection, COOKIE);

    if (XERR != NULL) {
        Debug::log(ERR, "Error in setupColormapAndStuff! Code: " + std::to_string(XERR->error_code));
    }

    free(XERR);
}

void CWindowManager::setupRandrMonitors() {

    XCBQUERYCHECK(RANDRVER, xcb_randr_query_version_reply(
        DisplayConnection, xcb_randr_query_version(DisplayConnection, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION), &errorRANDRVER), "RandR query failed!" );

        
    free(RANDRVER);

    Debug::log(LOG, "Setting up RandR! Query: v1.5.");

    XCBQUERYCHECK(MONITORS, xcb_randr_get_monitors_reply(DisplayConnection, xcb_randr_get_monitors(DisplayConnection, Screen->root, true), &errorMONITORS), "Couldn't get monitors. " + std::to_string(errorMONITORS->error_code));

    const auto MONITORNUM = xcb_randr_get_monitors_monitors_length(MONITORS);

    Debug::log(LOG, "Found " + std::to_string(MONITORNUM) + " Monitor(s)!");

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

    const auto EXTENSIONREPLY = xcb_get_extension_data(DisplayConnection, &xcb_randr_id);
    if (!EXTENSIONREPLY->present)
        Debug::log(ERR, "RandR extension missing");
    else {
        //listen for screen change events
        xcb_randr_select_input(DisplayConnection, Screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
        RandREventBase = EXTENSIONREPLY->first_event;
        Debug::log(LOG, "RandR first event base found at " + std::to_string(RandREventBase) + ".");
    }

    xcb_flush(DisplayConnection);

    if (monitors.size() == 0) {
        // RandR failed!
        Debug::log(WARN, "RandR failed!");
        monitors.clear();

#define TESTING_MON_AMOUNT 2
        for (int i = 0; i < TESTING_MON_AMOUNT /* Testing on 3 monitors, RandR shouldnt fail on a real desktop */; ++i) {
            monitors.push_back(SMonitor());
            monitors[i].vecPosition = Vector2D(i * Screen->width_in_pixels / TESTING_MON_AMOUNT, 0);
            monitors[i].vecSize = Vector2D(Screen->width_in_pixels / TESTING_MON_AMOUNT, Screen->height_in_pixels);
            monitors[i].ID = i;
            monitors[i].szName = "Screen" + std::to_string(i);
        }
    }
}

void CWindowManager::setupManager() {
    setupColormapAndStuff();
    EWMH::setupInitEWMH();

    // ---- RANDR ----- //
    setupRandrMonitors();

    Debug::log(LOG, "RandR done.");

    //
    //

    Values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(DisplayConnection, Screen->root,
                                         XCB_CW_EVENT_MASK, Values);

    Debug::log(LOG, "Root done.");

    ConfigManager::init();

    Debug::log(LOG, "Config done.");

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

    // ---- INIT THE THREAD FOR ANIM & CONFIG ---- //

    // start its' update thread
    Events::setThread();

    Debug::log(LOG, "Thread (Parent) done.");

    updateRootCursor();

    CWorkspace scratchpad;
    scratchpad.setID(SCRATCHPAD_ID);
    for (long unsigned int i = 0; i < monitors.size(); ++i) {
        if (monitors[i].primary)
            scratchpad.setMonitor(monitors[i].ID);
    }
    workspaces.push_back(scratchpad);

    Debug::log(LOG, "Finished setup!");

    // TODO: EWMH
}

bool CWindowManager::handleEvent() {
    if (xcb_connection_has_error(DisplayConnection))
        return false;

    xcb_flush(DisplayConnection);
    
    // recieve the event. Blocks.
    recieveEvent();

    // refresh and apply the parameters of all dirty windows.
    refreshDirtyWindows();

    // Sanity checks
    for (const auto active : activeWorkspaces) {
        sanityCheckOnWorkspace(active);
    }

    // hide ewmh bars if fullscreen
    processBarHiding();

    // remove unused workspaces
    cleanupUnusedWorkspaces();

    // Process the queued warp
    dispatchQueuedWarp();

    // Update last window name
    updateActiveWindowName();

    // Update the bar with the freshest stuff
    updateBarInfo();

    // Update EWMH workspace info
    EWMH::updateDesktops();

    xcb_flush(DisplayConnection);

    // Restore thread state
    mainThreadBusy = false;

    return true;
}

void CWindowManager::recieveEvent() {
    const auto ev = xcb_wait_for_event(DisplayConnection);
    if (ev != NULL) {
        while (animationUtilBusy) {
            ;  // wait for it to finish
        }

        for (auto& e : Events::ignoredEvents) {
            if (e == ev->sequence) {
                Debug::log(LOG, "Ignoring event type " + std::to_string(ev->response_type & ~0x80) + ".");
                free(ev);
                return;
            }
        }
                
        if (Events::ignoredEvents.size() > 20)
            Events::ignoredEvents.pop_front();

        // Set thread state, halt animations until done.
        mainThreadBusy = true;

        // Read from the bar
        if (!g_pWindowManager->statusBar)
            IPCRecieveMessageM(m_sIPCBarPipeOut.szPipeName);

        const uint8_t TYPE = XCB_EVENT_RESPONSE_TYPE(ev);
        const auto EVENTCODE = ev->response_type & ~0x80;

        switch (EVENTCODE) {
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
            case XCB_UNMAP_NOTIFY:
                Events::eventUnmapWindow(ev);
                Debug::log(LOG, "Event dispatched UNMAP");
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
                // Debug::log(LOG, "Event dispatched MOTION_NOTIFY"); // Spam!!
                break;
            case XCB_EXPOSE:
                Events::eventExpose(ev);
                Debug::log(LOG, "Event dispatched EXPOSE");
                break;
            case XCB_KEY_PRESS:
                Events::eventKeyPress(ev);
                Debug::log(LOG, "Event dispatched KEY_PRESS");
                break;
            case XCB_CLIENT_MESSAGE:
                Events::eventClientMessage(ev);
                Debug::log(LOG, "Event dispatched CLIENT_MESSAGE");
                break;
            case XCB_CONFIGURE_REQUEST:
                Events::eventConfigure(ev);
                Debug::log(LOG, "Event dispatched CONFIGURE");
                break;

            default:

                if ((EVENTCODE != 14) && (EVENTCODE != 13) && (EVENTCODE != 0) && (EVENTCODE != 22) && (TYPE - RandREventBase != XCB_RANDR_SCREEN_CHANGE_NOTIFY))
                    Debug::log(WARN, "Unknown event: " + std::to_string(ev->response_type & ~0x80));
                break;
        }

        if ((int)TYPE - RandREventBase == XCB_RANDR_SCREEN_CHANGE_NOTIFY && RandREventBase > 0) {
            Events::eventRandRScreenChange(ev);
            Debug::log(LOG, "Event dispatched RANDR_SCREEN_CHANGE");
        }

        free(ev);
    }
}

void CWindowManager::processBarHiding() {
    for (auto& w : windows) {
        if (!w.getDock())
            continue;

        // get the dock's monitor
        const auto& MON = monitors[w.getMonitor()];

        // get the dock's current workspace
        auto *const WORK = getWorkspaceByID(activeWorkspaces[MON.ID]);

        if (!WORK)
            continue; // weird if happens

        if (WORK->getHasFullscreenWindow() && !w.getDockHidden()) {
            const auto COOKIE = xcb_unmap_window(DisplayConnection, w.getDrawable());
            Events::ignoredEvents.push_back(COOKIE.sequence);
            w.setDockHidden(true);
        }
            
        else if (!WORK->getHasFullscreenWindow() && w.getDockHidden()) {
            xcb_map_window(DisplayConnection, w.getDrawable());
            w.setDockHidden(false);
        }
    }
}

void CWindowManager::cleanupUnusedWorkspaces() {
    std::deque<CWorkspace> temp = workspaces;

    workspaces.clear();

    for (auto& work : temp) {
        if (!isWorkspaceVisible(work.getID())) {
            // check if it has any children
            bool hasChildren = getWindowsOnWorkspace(work.getID()) > 0;

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
    const auto START = std::chrono::high_resolution_clock::now();
    for(auto& window : windows) {
        if (window.getDirty()) {
            window.setDirty(false);

            // Check if the window isn't a node or has the noInterventions prop
            if (window.getChildNodeAID() != 0 || window.getNoInterventions() || window.getDock()) 
                continue;
                
            setEffectiveSizePosUsingConfig(&window);

            const auto PWORKSPACE = getWorkspaceByID(window.getWorkspaceID());

            // Fullscreen flag
            bool bHasFullscreenWindow = PWORKSPACE ? PWORKSPACE->getHasFullscreenWindow() : false;

            // first and foremost, let's check if the window isn't on a hidden workspace
            // or an animated workspace
            if (PWORKSPACE && (!isWorkspaceVisible(window.getWorkspaceID())
                || PWORKSPACE->getAnimationInProgress()) && !window.getPinned()) {

                const auto MONITOR = getMonitorFromWindow(&window);

                Values[0] = (int)(window.getFullscreen() ? MONITOR->vecPosition.x : window.getRealPosition().x) + (int)PWORKSPACE->getCurrentOffset().x;
                Values[1] = (int)(window.getFullscreen() ? MONITOR->vecPosition.y : window.getRealPosition().y) + (int)PWORKSPACE->getCurrentOffset().y;

                if (bHasFullscreenWindow && !window.getFullscreen() && (window.getUnderFullscreen() || !window.getIsFloating())) {
                    Values[0] = 150000;
                    Values[1] = 150000;
                }

                if (VECTORDELTANONZERO(window.getLastUpdatePosition(), Vector2D(Values[0], Values[1]))) {
                    xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
                    window.setLastUpdatePosition(Vector2D(Values[0], Values[1]));
                }

                // Set the size JIC.
                Values[0] = window.getFullscreen() ? MONITOR->vecSize.x : (int)window.getEffectiveSize().x;
                Values[1] = window.getFullscreen() ? MONITOR->vecSize.y : (int)window.getEffectiveSize().y;
                if (VECTORDELTANONZERO(window.getLastUpdateSize(), Vector2D(Values[0], Values[1]))) {
                    xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
                    window.setLastUpdateSize(Vector2D(Values[0], Values[1]));
                }

                applyShapeToWindow(&window);

                continue;
            }

            // or that it is not a non-fullscreen window in a fullscreen workspace thats under
            if (bHasFullscreenWindow && !window.getFullscreen() && (window.getUnderFullscreen() || !window.getIsFloating()) && !window.getPinned()) {
                Values[0] = 150000;
                Values[1] = 150000;
                if (VECTORDELTANONZERO(window.getLastUpdatePosition(), Vector2D(Values[0], Values[1]))) {
                    xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
                    window.setLastUpdatePosition(Vector2D(Values[0], Values[1]));
                }

                continue;
            }

            // Fullscreen window. No border, all screen.
            // also do this when "layout:no_gaps_when_only" is set, but with a twist to enable the bar
            if (window.getFullscreen() || (ConfigManager::getInt("layout:no_gaps_when_only") && getWindowsOnWorkspace(window.getWorkspaceID()) == 1)) {
                Values[0] = 0;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                const auto MONITOR = getMonitorFromWindow(&window);

                Values[0] = window.getFullscreen() ? (int)MONITOR->vecSize.x : MONITOR->vecSize.x - MONITOR->vecReservedTopLeft.x - MONITOR->vecReservedBottomRight.x;
                Values[1] = window.getFullscreen() ? (int) MONITOR->vecSize.y : MONITOR->vecSize.y - MONITOR->vecReservedTopLeft.y - MONITOR->vecReservedBottomRight.y;
                window.setEffectiveSize(Vector2D(Values[0], Values[1]));

                Values[0] = window.getFullscreen() ? (int)MONITOR->vecPosition.x : MONITOR->vecPosition.x + MONITOR->vecReservedTopLeft.x;
                Values[1] = window.getFullscreen() ? (int)MONITOR->vecPosition.y : MONITOR->vecPosition.y + MONITOR->vecReservedTopLeft.y;
                window.setEffectivePosition(Vector2D(Values[0], Values[1]));

                Values[0] = (int)window.getRealPosition().x;
                Values[1] = (int)window.getRealPosition().y;
                if (VECTORDELTANONZERO(window.getLastUpdatePosition(), Vector2D(Values[0], Values[1]))) {
                    const auto COOKIE = xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
                    window.setLastUpdatePosition(Vector2D(Values[0], Values[1]));

                    Events::ignoredEvents.push_back(COOKIE.sequence);
                }
            } else {
                // Update the position because the border makes the window jump
                // I have added the bordersize vec2d before in the setEffectiveSizePosUsingConfig function.
                Values[0] = (int)window.getRealPosition().x - ConfigManager::getInt("border_size");
                Values[1] = (int)window.getRealPosition().y - ConfigManager::getInt("border_size");
                if (VECTORDELTANONZERO(window.getLastUpdatePosition(), Vector2D(Values[0], Values[1]))) {
                    const auto COOKIE = xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
                    window.setLastUpdatePosition(Vector2D(Values[0], Values[1]));

                    Events::ignoredEvents.push_back(COOKIE.sequence);
                }

                Values[0] = (int)ConfigManager::getInt("border_size");
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);

                Values[0] = window.getRealBorderColor().getAsUint32();
                xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);
            }

            // If it isn't animated or we have non-cheap animations, update the real size
            if (!window.getIsAnimated() || ConfigManager::getInt("animations:cheap") == 0) {
                Values[0] = (int)window.getRealSize().x;
                Values[1] = (int)window.getRealSize().y;
                if (VECTORDELTANONZERO(window.getLastUpdateSize(), Vector2D(Values[0], Values[1]))) {
                    const auto COOKIE = xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
                    window.setLastUpdateSize(Vector2D(Values[0], Values[1]));

                    Events::ignoredEvents.push_back(COOKIE.sequence);
                }
                window.setFirstAnimFrame(true);
            }

            if (ConfigManager::getInt("animations:cheap") == 1 && window.getFirstAnimFrame() && window.getIsAnimated()) {
                // first frame, fix the size if smaller
                window.setFirstAnimFrame(false);
                if (window.getRealSize().x < window.getEffectiveSize().x || window.getRealSize().y < window.getEffectiveSize().y) {
                    Values[0] = (int)window.getEffectiveSize().x;
                    Values[1] = (int)window.getEffectiveSize().y;
                    if (VECTORDELTANONZERO(window.getLastUpdateSize(), Vector2D(Values[0], Values[1]))) {
                        const auto COOKIE = xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
                        window.setLastUpdateSize(Vector2D(Values[0], Values[1]));

                        Events::ignoredEvents.push_back(COOKIE.sequence);
                    }
                }
            }

            applyShapeToWindow(&window);

            // EWMH
            EWMH::updateWindow(window.getDrawable());
        }
    }

    Debug::log(LOG, "Refreshed dirty windows in " + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - START).count()) + "us.");
}

void CWindowManager::setFocusedWindow(xcb_drawable_t window) {
    if (window && window != Screen->root) {
        const auto PNEWFOCUS = g_pWindowManager->getWindowFromDrawable(window);

        if (PNEWFOCUS && PNEWFOCUS->getNoInterventions()) {
            Debug::log(LOG, "Not setting focus to a non-interventions window.");
            return;
        }

        Debug::log(LOG, "Setting focus to " + std::to_string(window));

        xcb_ungrab_pointer(DisplayConnection, XCB_CURRENT_TIME);

        // border color
        if (const auto PLASTWIN = getWindowFromDrawable(LastWindow); PLASTWIN) {
            PLASTWIN->setEffectiveBorderColor(CFloatingColor(ConfigManager::getInt("col.inactive_border")));
        }
        if (const auto PLASTWIN = getWindowFromDrawable(window); PLASTWIN) {
		    PLASTWIN->setEffectiveBorderColor(CFloatingColor(ConfigManager::getInt("col.active_border")));
        }

        if (const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(window); PWINDOW) {
            // Apply rounded corners, does all the checks inside.
            // The border changed so let's not make it rectangular maybe
            applyShapeToWindow(PWINDOW);
        }

        const auto LASTWINID = LastWindow;

        LastWindow = window;

        if (PNEWFOCUS) {
            applyShapeToWindow(g_pWindowManager->getWindowFromDrawable(window));

            // Transients
            PNEWFOCUS->bringTopRecursiveTransients();
        }

        // set focus in X11
        xcb_set_input_focus(DisplayConnection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);

        // EWMH
        EWMH::updateCurrentWindow(window);

        EWMH::updateWindow(window);
        EWMH::updateWindow(LASTWINID);
    }
}

// TODO: make this executed less. It's too often imo.
void CWindowManager::sanityCheckOnWorkspace(int workspaceID) {
    for (auto& w : windows) {
        if (w.getWorkspaceID() == workspaceID) {
            
            // Check #1: Parent has 2 identical children (happens!)
            if (w.getDrawable() < 0) {
                const auto CHILDA = w.getChildNodeAID();
                const auto CHILDB = w.getChildNodeBID();

                if (CHILDA == CHILDB) {
                    // Fix. Remove this parent, replace with child.
                    Debug::log(LOG, "Sanity check A triggered for window ID " + std::to_string(w.getDrawable()));

                    const auto PCHILD = getWindowFromDrawable(CHILDA);

                    if (!PCHILD){
                        // Means both children are 0 (dead)
                        removeWindowFromVectorSafe(w.getDrawable());
                        continue;
                    }

                    PCHILD->setPosition(w.getPosition());
                    PCHILD->setSize(w.getSize());

                    // make the sibling replace the parent
                    PCHILD->setParentNodeID(w.getParentNodeID());

                    if (w.getParentNodeID() != 0 && getWindowFromDrawable(w.getParentNodeID())) {
                        if (getWindowFromDrawable(w.getParentNodeID())->getChildNodeAID() == w.getDrawable()) {
                            getWindowFromDrawable(w.getParentNodeID())->setChildNodeAID(w.getDrawable());
                        } else {
                            getWindowFromDrawable(w.getParentNodeID())->setChildNodeBID(w.getDrawable());
                        }
                    }

                    // Make the sibling eat the closed window
                    PCHILD->setDirtyRecursive(true);
                    PCHILD->recalcSizePosRecursive();

                    // Remove the parent
                    removeWindowFromVectorSafe(w.getDrawable());

                    if (findWindowAtCursor())
                        setFocusedWindow(findWindowAtCursor()->getDrawable());  // Set focus. :)

                    Debug::log(LOG, "Sanity check A finished successfully.");
                }
            }

            // Hypothetical check #2: Check if children are present and tiled. (for nodes)
            // I have not found this occurring but I have had some issues with... stuff.
            if (w.getDrawable() < 0) {
                const auto CHILDA = getWindowFromDrawable(w.getChildNodeAID());
                const auto CHILDB = getWindowFromDrawable(w.getChildNodeBID());

                if (CHILDA && CHILDB) {
                    
                    if (CHILDA->getIsFloating()) {
                        g_pWindowManager->fixWindowOnClose(CHILDA);
                        g_pWindowManager->calculateNewWindowParams(CHILDA);

                        Debug::log(LOG, "Found an invalid tiled window, ID: " + std::to_string(CHILDA->getDrawable()) + ", untiling it.");
                    }

                    if (CHILDB->getIsFloating()) {
                        g_pWindowManager->fixWindowOnClose(CHILDB);
                        g_pWindowManager->calculateNewWindowParams(CHILDB);

                        Debug::log(LOG, "Found an invalid tiled window, ID: " + std::to_string(CHILDB->getDrawable()) + ", untiling it.");
                    }

                } else {
                    Debug::log(ERR, "Malformed node ID " + std::to_string(w.getDrawable()) + " with 2 children but one or both are nullptr.");

                    // fix it
                    if (!CHILDA && !CHILDB) {
                        closeWindowAllChecks(w.getDrawable());
                        Debug::log(ERR, "Node fixed, both nullptr.");
                        continue;
                    }

                    const auto PNULLCHILD = CHILDA ? CHILDB : CHILDA;
                    const auto PSIBLING = CHILDA ? CHILDA : CHILDB;

                    const auto PPARENT = getWindowFromDrawable(w.getDrawable());

                    if (!PPARENT)
                        return;  // ????????

                    if (!PSIBLING) {
                        Debug::log(ERR, "No sibling found in fixing malformed node! (Corrupted tree...?)");
                        return;
                    }

                    // FIX TREE ----
                    // make the sibling replace the parent
                    PSIBLING->setPosition(PPARENT->getPosition());
                    PSIBLING->setSize(PPARENT->getSize());
                    PSIBLING->setParentNodeID(PPARENT->getParentNodeID());

                    if (PPARENT->getParentNodeID() != 0 && getWindowFromDrawable(PPARENT->getParentNodeID())) {
                        if (getWindowFromDrawable(PPARENT->getParentNodeID())->getChildNodeAID() == PPARENT->getDrawable()) {
                            getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeAID(PSIBLING->getDrawable());
                        } else {
                            getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeBID(PSIBLING->getDrawable());
                        }
                    }
                    // TREE FIXED ----
                    Debug::log(ERR, "Tree fixed.");

                    // Fix master stuff
                    getMasterForWorkspace(PSIBLING->getWorkspaceID());

                    // recalc the workspace
                    if (ConfigManager::getInt("layout") == LAYOUT_MASTER)
                        recalcEntireWorkspace(PSIBLING->getWorkspaceID());
                    else {
                        PSIBLING->recalcSizePosRecursive();
                        PSIBLING->setDirtyRecursive(true);
                    }

                    // Remove the parent
                    removeWindowFromVectorSafe(PPARENT->getDrawable());

                    if (findWindowAtCursor())
                        setFocusedWindow(findWindowAtCursor()->getDrawable());  // Set focus. :)


                    Debug::log(ERR, "Node fixed, one nullptr.");
                }
            }
        }
    }
}

CWindow* CWindowManager::getWindowFromDrawable(int64_t window) {
    if (!window)
        return nullptr;

    for (auto& w : windows) {
        if (w.getDrawable() == window) {
            return &w;
        }
    }

    for (auto& w : unmappedWindows) {
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

    std::deque<CWindow> temp = windows;

    windows.clear();
    
    for(auto p : temp) {
        if (p.getDrawable() != window) {
            windows.push_back(p);
            continue;
        }
    }
}

void CWindowManager::applyShapeToWindow(CWindow* pWindow) {
    if (!pWindow)
        return;

    const auto ROUNDING = pWindow->getFullscreen() || (ConfigManager::getInt("layout:no_gaps_when_only") && getWindowsOnWorkspace(pWindow->getWorkspaceID()) == 1) ? 0 : ConfigManager::getInt("rounding");

    const auto SHAPEQUERY = xcb_get_extension_data(DisplayConnection, &xcb_shape_id);

    if (!SHAPEQUERY || !SHAPEQUERY->present || pWindow->getNoInterventions())
        return;

    Debug::log(LOG, "Applying shape to " + std::to_string(pWindow->getDrawable()));

    // Prepare values

    const auto MONITOR = getMonitorFromWindow(pWindow);

    if (!MONITOR) {
        Debug::log(ERR, "No monitor for " + std::to_string(pWindow->getDrawable()) + "??");
        return;
    }

    const uint16_t W = pWindow->getFullscreen() ? MONITOR->vecSize.x : pWindow->getRealSize().x;
    const uint16_t H = pWindow->getFullscreen() ? MONITOR->vecSize.y : pWindow->getRealSize().y;
    const uint16_t BORDER = pWindow->getFullscreen() || (ConfigManager::getInt("layout:no_gaps_when_only") && getWindowsOnWorkspace(pWindow->getWorkspaceID()) == 1) ? 0 : ConfigManager::getInt("border_size");
    const uint16_t TOTALW = W + 2 * BORDER;
    const uint16_t TOTALH = H + 2 * BORDER;

    const auto RADIUS = ROUNDING + BORDER;
    const auto DIAMETER = RADIUS == 0 ? 0 : RADIUS * 2 - 1;

    const xcb_arc_t BOUNDINGARCS[] = {
        {-1, -1, DIAMETER, DIAMETER, 0, 360 << 6},
        {-1, TOTALH - DIAMETER, DIAMETER, DIAMETER, 0, 360 << 6},
        {TOTALW - DIAMETER, -1, DIAMETER, DIAMETER, 0, 360 << 6},
        {TOTALW - DIAMETER, TOTALH - DIAMETER, DIAMETER, DIAMETER, 0, 360 << 6},
    };
    const xcb_rectangle_t BOUNDINGRECTS[] = {
        {RADIUS, 0, TOTALW - DIAMETER, TOTALH},
        {0, RADIUS, TOTALW, TOTALH - DIAMETER},
    };

    const auto DIAMETERC = ROUNDING == 0 ? 0 : 2 * ROUNDING - 1;

    xcb_arc_t CLIPPINGARCS[] = {
        {-1, -1, DIAMETERC, DIAMETERC, 0, 360 << 6},
        {-1, H - DIAMETERC, DIAMETERC, DIAMETERC, 0, 360 << 6},
        {W - DIAMETERC, -1, DIAMETERC, DIAMETERC, 0, 360 << 6},
        {W - DIAMETERC, H - DIAMETERC, DIAMETERC, DIAMETERC, 0, 360 << 6},
    };
    xcb_rectangle_t CLIPPINGRECTS[] = {
        {ROUNDING, 0, W - DIAMETERC, H},
        {0, ROUNDING, W, H - DIAMETERC},
    };

    // Values done
    
    // XCB

    const xcb_pixmap_t PIXMAP1 = xcb_generate_id(DisplayConnection);
    const xcb_pixmap_t PIXMAP2 = xcb_generate_id(DisplayConnection);

    const xcb_gcontext_t BLACK = xcb_generate_id(DisplayConnection);
    const xcb_gcontext_t WHITE = xcb_generate_id(DisplayConnection);

    xcb_create_pixmap(DisplayConnection, 1, PIXMAP1, pWindow->getDrawable(), TOTALW, TOTALH);
    xcb_create_pixmap(DisplayConnection, 1, PIXMAP2, pWindow->getDrawable(), W, H);

    Values[0] = 0;
    Values[1] = 0;
    xcb_create_gc(DisplayConnection, BLACK, PIXMAP1, XCB_GC_FOREGROUND, Values);
    Values[0] = 1;
    xcb_create_gc(DisplayConnection, WHITE, PIXMAP1, XCB_GC_FOREGROUND, Values);

    // XCB done

    // Draw

    xcb_rectangle_t BOUNDINGRECT = {0, 0, W + 2 * BORDER, H + 2 * BORDER};
    xcb_poly_fill_rectangle(DisplayConnection, PIXMAP1, BLACK, 1, &BOUNDINGRECT);
    xcb_poly_fill_rectangle(DisplayConnection, PIXMAP1, WHITE, 2, BOUNDINGRECTS);
    xcb_poly_fill_arc(DisplayConnection, PIXMAP1, WHITE, 4, BOUNDINGARCS);

    xcb_rectangle_t CLIPPINGRECT = {0, 0, W, H};
    xcb_poly_fill_rectangle(DisplayConnection, PIXMAP2, BLACK, 1, &CLIPPINGRECT);
    xcb_poly_fill_rectangle(DisplayConnection, PIXMAP2, WHITE, 2, CLIPPINGRECTS);
    xcb_poly_fill_arc(DisplayConnection, PIXMAP2, WHITE, 4, CLIPPINGARCS);

    const auto WORKSPACE = getWorkspaceByID(pWindow->getWorkspaceID());

    if (!WORKSPACE) {
        Debug::log(ERR, "No workspace for " + std::to_string(pWindow->getDrawable()) + "??");
        return;
    }

    if (WORKSPACE->getAnimationInProgress()) {
        // if it's animated we draw 2 more black rects to clip it. (if it goes out of the monitor)

        if (W + (pWindow->getRealPosition().x + WORKSPACE->getCurrentOffset().x - MONITOR->vecPosition.x) > MONITOR->vecSize.x) {
            // clip right
            xcb_rectangle_t rect[] = {{MONITOR->vecSize.x - (pWindow->getRealPosition().x + WORKSPACE->getCurrentOffset().x - MONITOR->vecPosition.x), -100, W + 100, H + 100}};
            xcb_poly_fill_rectangle(DisplayConnection, PIXMAP1, BLACK, 1, rect);
            xcb_poly_fill_rectangle(DisplayConnection, PIXMAP2, BLACK, 1, rect);
        }

        if (pWindow->getRealPosition().x + WORKSPACE->getCurrentOffset().x - MONITOR->vecPosition.x < 0) {
            // clip left
            xcb_rectangle_t rect[] = {{-100, -100, - (pWindow->getRealPosition().x  + WORKSPACE->getCurrentOffset().x - MONITOR->vecPosition.x), H + 100}};
            xcb_poly_fill_rectangle(DisplayConnection, PIXMAP1, BLACK, 1, rect);
            xcb_poly_fill_rectangle(DisplayConnection, PIXMAP2, BLACK, 1, rect);
        }
    }

    // Draw done

    // Shape

    xcb_shape_mask(DisplayConnection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, pWindow->getDrawable(), -BORDER, -BORDER, PIXMAP1);
    xcb_shape_mask(DisplayConnection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_CLIP, pWindow->getDrawable(), 0, 0, PIXMAP2);

    // Shape done

    // Cleanup

    xcb_free_pixmap(DisplayConnection, PIXMAP1);
    xcb_free_pixmap(DisplayConnection, PIXMAP2);
}

void CWindowManager::setEffectiveSizePosUsingConfig(CWindow* pWindow) {
    if (!pWindow || pWindow->getIsFloating())
        return;

    const auto MONITOR = getMonitorFromWindow(pWindow);

    // set some flags.
    const bool DISPLAYLEFT          = STICKS(pWindow->getPosition().x, MONITOR->vecPosition.x);
    const bool DISPLAYRIGHT         = STICKS(pWindow->getPosition().x + pWindow->getSize().x, MONITOR->vecPosition.x + MONITOR->vecSize.x);
    const bool DISPLAYTOP           = STICKS(pWindow->getPosition().y, MONITOR->vecPosition.y);
    const bool DISPLAYBOTTOM        = STICKS(pWindow->getPosition().y + pWindow->getSize().y, MONITOR->vecPosition.y + MONITOR->vecSize.y);

    const auto BORDERSIZE = ConfigManager::getInt("border_size");
    const auto GAPSOUT = ConfigManager::getInt("gaps_out");
    const auto GAPSIN = ConfigManager::getInt("gaps_in");

    auto TEMPEFFECTIVESIZE = pWindow->getSize();
    auto TEMPEFFECTIVEPOS  = pWindow->getPosition();

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? GAPSOUT + MONITOR->vecReservedTopLeft.x : GAPSIN,
                                        DISPLAYTOP ? GAPSOUT + MONITOR->vecReservedTopLeft.y : GAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? GAPSOUT + MONITOR->vecReservedBottomRight.x : GAPSIN,
                                            DISPLAYBOTTOM ? GAPSOUT + MONITOR->vecReservedBottomRight.y : GAPSIN);

    TEMPEFFECTIVEPOS = TEMPEFFECTIVEPOS + Vector2D(BORDERSIZE, BORDERSIZE);
    TEMPEFFECTIVESIZE = TEMPEFFECTIVESIZE - (Vector2D(BORDERSIZE, BORDERSIZE) * 2);

    // do gaps, set top left
    TEMPEFFECTIVEPOS = TEMPEFFECTIVEPOS + OFFSETTOPLEFT;
    // fix to old size bottom right
    TEMPEFFECTIVESIZE = TEMPEFFECTIVESIZE - OFFSETTOPLEFT;
    // set bottom right
    TEMPEFFECTIVESIZE = TEMPEFFECTIVESIZE - OFFSETBOTTOMRIGHT;

    if (pWindow->getIsPseudotiled()) {
        float scale = 1;

        // adjust if doesnt fit
        if (pWindow->getPseudoSize().x > TEMPEFFECTIVESIZE.x || pWindow->getPseudoSize().y > TEMPEFFECTIVESIZE.y) {
            
            if (pWindow->getPseudoSize().x > TEMPEFFECTIVESIZE.x) {
                scale = TEMPEFFECTIVESIZE.x / pWindow->getPseudoSize().x;
            }

            if (pWindow->getPseudoSize().y * scale > TEMPEFFECTIVESIZE.y) {
                scale = TEMPEFFECTIVESIZE.y / pWindow->getPseudoSize().y;
            }

            auto DELTA = TEMPEFFECTIVESIZE - pWindow->getPseudoSize() * scale;
            TEMPEFFECTIVESIZE = pWindow->getPseudoSize() * scale;
            TEMPEFFECTIVEPOS = TEMPEFFECTIVEPOS + DELTA / 2.f;  // center
        } else {
            auto DELTA = TEMPEFFECTIVESIZE - pWindow->getPseudoSize();
            TEMPEFFECTIVEPOS = TEMPEFFECTIVEPOS + DELTA / 2.f;  // center
            TEMPEFFECTIVESIZE = pWindow->getPseudoSize();
        }
    }

    if (pWindow->getWorkspaceID() == SCRATCHPAD_ID) {
        TEMPEFFECTIVEPOS = TEMPEFFECTIVEPOS + ((TEMPEFFECTIVESIZE - TEMPEFFECTIVESIZE * 0.75f) * 0.5f);
        TEMPEFFECTIVESIZE = TEMPEFFECTIVESIZE * 0.75f;

        setAWindowTop(pWindow->getDrawable());
    }

    if (pWindow->getFullscreen()) {
        TEMPEFFECTIVEPOS = MONITOR->vecPosition;
        TEMPEFFECTIVESIZE = MONITOR->vecSize;
    }

    pWindow->setEffectivePosition(TEMPEFFECTIVEPOS);
    pWindow->setEffectiveSize(TEMPEFFECTIVESIZE);
}

CWindow* CWindowManager::findWindowAtCursor() {
    Vector2D cursorPos = getCursorPos();

    if (!getMonitorFromCursor())
        return nullptr;

    const auto WORKSPACE = activeWorkspaces[getMonitorFromCursor()->ID];

    for (auto& window : windows) {
        if (window.getWorkspaceID() == WORKSPACE && !window.getIsFloating() && window.getDrawable() > 0 && window.getConstructed() && window.getWorkspaceID() != SCRATCHPAD_ID) {

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

CWindow* CWindowManager::findFirstWindowOnWorkspace(const int& work) {
    for (auto& w : windows) {
        if (w.getWorkspaceID() == work && !w.getIsFloating() && !w.getNoInterventions() && w.getDrawable() > 0) {
            return &w;
        }
    }

    return nullptr;
}

CWindow* CWindowManager::findPreferredOnScratchpad() {
    Vector2D topSize;
    CWindow* pTop = nullptr;

    for (auto& w : windows) {
        if (w.getWorkspaceID() == SCRATCHPAD_ID && w.getDrawable() > 0 && w.getConstructed()) {
            if (w.getSize().x * w.getSize().y > topSize.x * topSize.y) {
                topSize = w.getSize();
                pTop = &w;
            }
        }
    }

    return pTop;
}

void CWindowManager::calculateNewTileSetOldTile(CWindow* pWindow) {
    
    // Get the parent and both children, one of which will be pWindow
    const auto PPARENT = getWindowFromDrawable(pWindow->getParentNodeID());

    auto PMONITOR = getMonitorFromWindow(pWindow);
    if (!PMONITOR) {
        Debug::log(ERR, "Monitor was nullptr! (calculateNewTileSetOldTile) using 0.");
        PMONITOR = &monitors[0];

        if (monitors.size() == 0) {
            Debug::log(ERR, "Not continuing. Monitors size 0.");
            return;
        } 
    }

    if (!PPARENT) {
        // New window on this workspace.
        // Open a fullscreen window.

        pWindow->setSize(Vector2D(PMONITOR->vecSize.x, PMONITOR->vecSize.y));
        pWindow->setPosition(Vector2D(PMONITOR->vecPosition.x, PMONITOR->vecPosition.y));

        return;
    }


    switch (ConfigManager::getInt("layout"))
    {
    case LAYOUT_DWINDLE:
        {
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
                Debug::log(ERR, "Sibling node was null?? pWindow x,y,w,h: " + std::to_string(pWindow->getPosition().x) + " " + std::to_string(pWindow->getPosition().y) + " " + std::to_string(pWindow->getSize().x) + " " + std::to_string(pWindow->getSize().y));
            }
        }
        break;
    
    case LAYOUT_MASTER:
        {
            recalcEntireWorkspace(pWindow->getWorkspaceID());
        }
        break;
    }
}

int CWindowManager::getWindowsOnWorkspace(const int& workspace) {
    int number = 0;
    for (auto& w : windows) {
        if (w.getWorkspaceID() == workspace && w.getDrawable() > 0 && !w.getDock()) {
            ++number;
        }
    }

    return number;
}

SMonitor* CWindowManager::getMonitorFromWorkspace(const int& workspace) {
    int monitorID = -1;
    for (auto& work : workspaces) {
        if (work.getID() == workspace) {
            monitorID = work.getMonitor();
            break;
        }
    }

    for (auto& monitor : monitors) {
        if (monitor.ID == monitorID) {
            return &monitor;
        }
    }

    return nullptr;
}

void CWindowManager::recalcEntireWorkspace(const int& workspace) {

    switch (ConfigManager::getInt("layout"))
    {
    case LAYOUT_MASTER:
        {
            // Get the monitor
            const auto PMONITOR = getMonitorFromWorkspace(workspace);

            // first, calc the size
            CWindow* pMaster = nullptr;
            for (auto& w : windows) {
                if (w.getWorkspaceID() == workspace && w.getMaster() && !w.getDead() && !w.getIsFloating() && !w.getDock()) {
                    pMaster = &w;
                    break;
                }
            }

            CWindow* pMasterContainer = nullptr;
            for (auto& w : windows) {
                if (w.getWorkspaceID() == workspace && w.getParentNodeID() == 0 && !w.getIsFloating() && !w.getDock()) {
                    pMasterContainer = &w;
                    break;
                }
            }

            if (!pMaster) {
                Debug::log(ERR, "No master found on workspace???");
                return;
            }

            // set the xy for master
            float splitRatio = 1;
            if (pMasterContainer)
                splitRatio = pMasterContainer->getSplitRatio();

            pMaster->setPosition(Vector2D(0, 0) + PMONITOR->vecPosition);
            pMaster->setSize(Vector2D(PMONITOR->vecSize.x / 2 * splitRatio, PMONITOR->vecSize.y));

            // get children sorted
            std::vector<CWindow*> children;
            for (auto& w : windows) {
                if (w.getWorkspaceID() == workspace && !w.getMaster() && w.getDrawable() > 0 && !w.getDead() && !w.getDock())
                    children.push_back(&w);
            }
            std::sort(children.begin(), children.end(), [](CWindow*& a, CWindow*& b) {
                return a->getMasterChildIndex() < b->getMasterChildIndex();
            });

            // if no children, master full
            if (children.size() == 0) {
                pMaster->setPosition(Vector2D(0, 0) + PMONITOR->vecPosition);
                pMaster->setSize(Vector2D(PMONITOR->vecSize.x, PMONITOR->vecSize.y));
            }

            // Children sorted, set xy
            int yoff = 0;
            for (const auto& child : children) {
                child->setPosition(Vector2D(PMONITOR->vecSize.x / 2 * splitRatio, yoff) + PMONITOR->vecPosition);
                child->setSize(Vector2D(PMONITOR->vecSize.x / 2 * (2 - splitRatio), PMONITOR->vecSize.y / children.size()));

                yoff += PMONITOR->vecSize.y / children.size();
            }

            // done
            setAllWorkspaceWindowsDirtyByID(workspace);
        }
        break;

    case LAYOUT_DWINDLE:
        {
            // get the master on the workspace
            CWindow* pMasterWindow = nullptr;
            for (auto& w : windows) {
                if (w.getWorkspaceID() == workspace && w.getParentNodeID() == 0 && !w.getIsFloating() && !w.getDock()) {
                    pMasterWindow = &w;
                    break;
                }
            }

            if (!pMasterWindow)
                return;

            const auto PMONITOR = getMonitorFromWorkspace(workspace);

            if (!PMONITOR)
                return;

            Debug::log(LOG, "Recalc for workspace " + std::to_string(workspace));

            pMasterWindow->setSize(PMONITOR->vecSize);
            pMasterWindow->setPosition(PMONITOR->vecPosition);

            pMasterWindow->recalcSizePosRecursive();
            setAllWorkspaceWindowsDirtyByID(workspace);
        }
        break;
    
    default:
        break;
    }
    
}

void CWindowManager::calculateNewFloatingWindow(CWindow* pWindow) {
    if (!pWindow)
        return;

    if (!pWindow->getNoInterventions() && !pWindow->getDock()) {
        pWindow->setPosition(pWindow->getEffectivePosition() + Vector2D(3,3));
        pWindow->setSize(pWindow->getEffectiveSize() - Vector2D(6, 6));

        // min size
        pWindow->setSize(Vector2D(std::clamp(pWindow->getSize().x, (double)40, (double)99999),
                                  std::clamp(pWindow->getSize().y, (double)40, (double)99999)));

        pWindow->setEffectivePosition(pWindow->getPosition() + Vector2D(10, 10));
        pWindow->setEffectiveSize(pWindow->getSize());

        pWindow->setRealPosition(pWindow->getPosition());
        pWindow->setRealSize(pWindow->getSize());
    }

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

void CWindowManager::closeWindowAllChecks(int64_t id) {
    // fix last window if tile
    const auto CLOSEDWINDOW = g_pWindowManager->getWindowFromDrawable(id);

    if (!CLOSEDWINDOW)
        return; // It's not in the vec, ignore. (weird)

    CLOSEDWINDOW->setDead(true);

    if (CLOSEDWINDOW->getWorkspaceID() != SCRATCHPAD_ID && scratchpadActive)
        scratchpadActive = false;

    if (const auto WORKSPACE = getWorkspaceByID(CLOSEDWINDOW->getWorkspaceID()); WORKSPACE && CLOSEDWINDOW->getFullscreen())
        WORKSPACE->setHasFullscreenWindow(false);

    if (!CLOSEDWINDOW->getIsFloating())
        g_pWindowManager->fixWindowOnClose(CLOSEDWINDOW);
    
    const bool WASDOCK = CLOSEDWINDOW->getDock();

    // delete off of the arr
    g_pWindowManager->removeWindowFromVectorSafe(id);

    // Fix docks
    if (WASDOCK)
        g_pWindowManager->recalcAllDocks();
}

CWindow* CWindowManager::getMasterForWorkspace(const int& work) {
    CWindow* pMaster = nullptr;
    for (auto& w : windows) {
        if (w.getWorkspaceID() == work && w.getMaster()) {
            pMaster = &w;
            break;
        }
    }

    if (!pMaster) {
        Debug::log(ERR, "No master found on workspace? Setting automatically");
        for (auto& w : windows) {
            if (w.getWorkspaceID() == work && !w.getDock() && !w.getIsFloating()) {
                pMaster = &w;
                w.setMaster(true);
                break;
            }
        }
    }

    return pMaster;
}

void CWindowManager::fixMasterWorkspaceOnClosed(CWindow* pWindow) {

    getMasterForWorkspace(pWindow->getWorkspaceID()); // to fix if no master

    // get children sorted
    std::vector<CWindow*> children;
    for (auto& w : windows) {
        if (w.getWorkspaceID() == pWindow->getWorkspaceID() && !w.getMaster() && w.getDrawable() > 0 && w.getDrawable() != pWindow->getDrawable() && !w.getDock())
            children.push_back(&w);
    }
    std::sort(children.begin(), children.end(), [](CWindow*& a, CWindow*& b) {
        return a->getMasterChildIndex() < b->getMasterChildIndex();
    });

    // If closed window was master, set a new master.
    if (pWindow->getMaster()) {
        if (children.size() > 0) {
            children[0]->setMaster(true);
        }
    } else {
        // else fix the indices
        for (long unsigned int i = pWindow->getMasterChildIndex() - 1; i < children.size(); ++i) {
            // masterChildIndex = 1 for the first child
            children[i]->setMasterChildIndex(i);
        }
    }
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

    // used by master layout
    pClosedWindow->setDead(true);

    // FIX TREE ----
    // make the sibling replace the parent
    PSIBLING->setPosition(PPARENT->getPosition());
    PSIBLING->setSize(PPARENT->getSize());
    PSIBLING->setParentNodeID(PPARENT->getParentNodeID());

    if (PPARENT->getParentNodeID() != 0
        && getWindowFromDrawable(PPARENT->getParentNodeID())) {
            if (getWindowFromDrawable(PPARENT->getParentNodeID())->getChildNodeAID() == PPARENT->getDrawable()) {
                getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeAID(PSIBLING->getDrawable());
            } else {
                getWindowFromDrawable(PPARENT->getParentNodeID())->setChildNodeBID(PSIBLING->getDrawable());
            }
    }
    // TREE FIXED ----

    // Fix master stuff
    const auto WORKSPACE = pClosedWindow->getWorkspaceID();
    fixMasterWorkspaceOnClosed(pClosedWindow);

    // recalc the workspace
    if (ConfigManager::getInt("layout") == LAYOUT_MASTER)
        recalcEntireWorkspace(WORKSPACE);
    else {
        PSIBLING->recalcSizePosRecursive();
        PSIBLING->setDirtyRecursive(true);
    }
        
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

    auto longestIntersect = -1;
    CWindow* longestIntersectWindow = nullptr;

    for (auto& w : windows) {
        if (w.getDrawable() == CURRENTWINDOW->getDrawable() || w.getDrawable() < 1 || w.getIsFloating() || !isWorkspaceVisible(w.getWorkspaceID()))
            continue;

        const auto POSB = w.getPosition();
        const auto SIZEB = w.getSize();

        switch (dir) {
            case 'l':
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }       
                }
                break;
            case 'r':
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
            case 't':
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
            case 'b':
            case 'd':
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max((double)0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect = INTERSECTLEN;
                        longestIntersectWindow = &w;
                    }
                }
                break;
        }
    }

    if (longestIntersect != -1)
        return longestIntersectWindow;

    return nullptr;
}

void CWindowManager::warpCursorTo(Vector2D to) {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    xcb_query_pointer_reply_t* pointerreply = xcb_query_pointer_reply(DisplayConnection, POINTERCOOKIE, NULL);
    if (!pointerreply) {
        Debug::log(ERR, "Couldn't query pointer.");
        free(pointerreply);
        return;
    }

    xcb_warp_pointer(DisplayConnection, XCB_NONE, Screen->root, 0, 0, Screen->width_in_pixels, Screen->height_in_pixels, (int)to.x, (int)to.y);
    free(pointerreply);
}

void CWindowManager::moveActiveWindowToWorkspace(int workspace) {

    auto PWINDOW = getWindowFromDrawable(LastWindow);

    if (!PWINDOW)
        return;

    if (PWINDOW->getWorkspaceID() == workspace)
        return;

    Debug::log(LOG, "Moving active window to " + std::to_string(workspace));

    const auto SAVEDDEFAULTSIZE = PWINDOW->getDefaultSize();
    const auto SAVEDFLOATSTATUS = PWINDOW->getIsFloating();
    const auto SAVEDDRAWABLE    = PWINDOW->getDrawable();

    // remove current workspace's fullscreen status if fullscreen
    if (PWINDOW->getFullscreen()) {
        const auto PWORKSPACE = getWorkspaceByID(PWINDOW->getWorkspaceID());
        if (PWORKSPACE) {
            PWORKSPACE->setHasFullscreenWindow(false);
        }
    }

    fixWindowOnClose(PWINDOW);
    // deque reallocated
    LastWindow = SAVEDDRAWABLE;
    PWINDOW = getWindowFromDrawable(LastWindow);
    PWINDOW->setDead(false);

    const auto WORKSPACE = getWorkspaceByID(PWINDOW->getWorkspaceID());

    auto workspacesBefore = activeWorkspaces;

    if (WORKSPACE && PWINDOW->getFullscreen())
        WORKSPACE->setHasFullscreenWindow(false);

    changeWorkspaceByID(workspace);

    // Find new mon
    int NEWMONITOR = 0;
    for (long unsigned int i = 0; i < activeWorkspaces.size(); ++i) {
        if (workspace == SCRATCHPAD_ID) {
            if (monitors[i].ID == ConfigManager::getInt("scratchpad_mon"))
                NEWMONITOR = i;
        }
        else if (activeWorkspaces[i] == workspace) {
            NEWMONITOR = i;
        }
    }

    // Find the first window on the new workspace
    xcb_drawable_t newLastWindow = 0;
    for (auto& w : windows) {
        if (w.getDrawable() > 0 && w.getWorkspaceID() == workspace && !w.getIsFloating()) {
            newLastWindow = w.getDrawable();
            break;
        }
    }

    const auto LASTFOCUS = LastWindow;

    if (newLastWindow) {
        setFocusedWindow(newLastWindow);
    }

    PWINDOW->setConstructed(false);

    if (SAVEDFLOATSTATUS && workspace != SCRATCHPAD_ID)
        Events::remapFloatingWindow(PWINDOW->getDrawable(), NEWMONITOR);
    else
        Events::remapWindow(PWINDOW->getDrawable(), false, NEWMONITOR);

    PWINDOW->setConstructed(true);

    // fix for scratchpad
    PWINDOW->setWorkspaceID(workspace);

    // fix fullscreen status
    const auto PWORKSPACE = getWorkspaceByID(workspace);
    if (PWORKSPACE) {
        // Should NEVER be false but let's be sure
        PWORKSPACE->setHasFullscreenWindow(PWINDOW->getFullscreen());
    }

    PWINDOW->setDefaultSize(SAVEDDEFAULTSIZE);

    if (workspace == SCRATCHPAD_ID) {
        for (int i = 0; i < activeWorkspaces.size(); ++i) {
            changeWorkspaceByID(workspacesBefore[i]);
        }

        changeWorkspaceByID(WORKSPACE->getID());
        setFocusedWindow(LASTFOCUS);
    }

    QueuedPointerWarp = Vector2D(-1,-1);
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

    // Fix the tree
    if (CURRENTWINDOW->getParentNodeID() != 0) {
        const auto PPARENT = getWindowFromDrawable(CURRENTWINDOW->getParentNodeID());
        if (!PPARENT) {
            Debug::log(ERR, "No parent node ID despite non-null???");
            return;
        }

        if (PPARENT->getChildNodeAID() == CURRENTWINDOW->getDrawable())
            PPARENT->setChildNodeAID(neighbor->getDrawable());
        else
            PPARENT->setChildNodeBID(neighbor->getDrawable());
    }

    if (neighbor->getParentNodeID() != 0) {
        const auto PPARENT = getWindowFromDrawable(neighbor->getParentNodeID());
        if (!PPARENT) {
            Debug::log(ERR, "No parent node ID despite non-null???");
            return;
        }

        if (PPARENT->getChildNodeAID() == neighbor->getDrawable())
            PPARENT->setChildNodeAID(CURRENTWINDOW->getDrawable());
        else
            PPARENT->setChildNodeBID(CURRENTWINDOW->getDrawable());
    }

    const auto PARENTC = CURRENTWINDOW->getParentNodeID();

    CURRENTWINDOW->setParentNodeID(neighbor->getParentNodeID());
    neighbor->setParentNodeID(PARENTC);

    // Fix the master layout
    const auto SMASTER = CURRENTWINDOW->getMaster();
    const auto SINDEX  = CURRENTWINDOW->getMasterChildIndex();
    
    CURRENTWINDOW->setMasterChildIndex(neighbor->getMasterChildIndex());
    CURRENTWINDOW->setMaster(neighbor->getMaster());

    neighbor->setMaster(SMASTER);
    neighbor->setMasterChildIndex(SINDEX);
        

    // finish by moving the cursor to the new current window
    QueuedPointerWarp = Vector2D(CURRENTWINDOW->getPosition() + CURRENTWINDOW->getSize() / 2.f);
}

CWindow* CWindowManager::getFullscreenWindowByWorkspace(const int& id) {
    for (auto& window : windows) {
        if (window.getWorkspaceID() == id && window.getFullscreen() && window.getDrawable() > 0)
            return &window;
    }

    return nullptr;
}

void CWindowManager::moveActiveFocusTo(char dir) {
    const auto CURRENTWINDOW = getWindowFromDrawable(LastWindow);

    if (!CURRENTWINDOW)
        return;

    const auto PWORKSPACE = getWorkspaceByID(CURRENTWINDOW->getWorkspaceID());

    if (!PWORKSPACE)
        return;

    const auto NEIGHBOR = PWORKSPACE->getHasFullscreenWindow() ? getFullscreenWindowByWorkspace(PWORKSPACE->getID()) : getNeighborInDir(dir);

    if (!NEIGHBOR)
        return;

    // move the focus
    setFocusedWindow(NEIGHBOR->getDrawable());

    // finish by moving the cursor to the neighbor window
    QueuedPointerWarp = Vector2D(NEIGHBOR->getPosition() + (NEIGHBOR->getSize() / 2.f));
}

void CWindowManager::changeWorkspaceByID(int ID) {

    auto MONITOR = getMonitorFromCursor();

    if (!MONITOR) {
        Debug::log(ERR, "Monitor was nullptr! (changeWorkspaceByID) Using monitor 0.");
        MONITOR = &monitors[0];
    }

    // Don't change if already opened
    if (isWorkspaceVisible(ID)) {
        Debug::log(LOG, "Workspace visible, only focus.");
        focusOnWorkspace(ID);
        return;
    }
        

    // mark old workspace dirty
    setAllWorkspaceWindowsDirtyByID(activeWorkspaces[MONITOR->ID]);

    // save old workspace for anim
    auto OLDWORKSPACE = activeWorkspaces[MONITOR->ID];
    lastActiveWorkspaceID = OLDWORKSPACE;

    for (auto& workspace : workspaces) {
        if (workspace.getID() == ID) {
            Debug::log(LOG, "Workspace open, bringing to active.");

            // set workspaces dirty
            setAllWorkspaceWindowsDirtyByID(activeWorkspaces[workspace.getMonitor()]);
            setAllWorkspaceWindowsDirtyByID(ID);

            OLDWORKSPACE = activeWorkspaces[workspace.getMonitor()];
            activeWorkspaces[workspace.getMonitor()] = workspace.getID();

            // if not fullscreen set the focus to any window on that workspace
            // if fullscreen, set to the fullscreen window
            focusOnWorkspace(ID);

            // Update bar info
            updateBarInfo();

            Debug::log(LOG, "Bar info updated with workspace changed.");

            // Wipe animation
            startWipeAnimOnWorkspace(OLDWORKSPACE, ID);
            
            return;
        }
    }

    Debug::log(LOG, "New workspace, creating.");

    // If we are here it means the workspace is new. Let's create it.
    CWorkspace newWorkspace;
    newWorkspace.setID(ID);
    newWorkspace.setMonitor(MONITOR->ID);
    workspaces.push_back(newWorkspace);
    activeWorkspaces[MONITOR->ID] = workspaces[workspaces.size() - 1].getID();
    LastWindow = -1;

    // Update bar info
    updateBarInfo();

    // Wipe animation
    startWipeAnimOnWorkspace(OLDWORKSPACE, ID);

    Debug::log(LOG, "New workspace created.");

    if (getMonitorFromCursor() && MONITOR->ID != getMonitorFromCursor()->ID)
        QueuedPointerWarp = Vector2D(MONITOR->vecPosition + MONITOR->vecSize / 2.f);

    // no need for the new dirty, it's empty
}

void CWindowManager::changeToLastWorkspace() {
    changeWorkspaceByID(lastActiveWorkspaceID);
}

void CWindowManager::focusOnWorkspace(const int& work) {
    const auto PWORKSPACE = getWorkspaceByID(work);
    const auto PMONITOR = getMonitorFromWorkspace(work);

    Debug::log(LOG, "Focusing on workspace " + std::to_string(work));

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned workspace at focusOnWorkspace ???");
        return;
    }

    if (PWORKSPACE) {
        if (!PWORKSPACE->getHasFullscreenWindow()) {
            Debug::log(LOG, "No fullscreen window");
            bool shouldHopToScreen = true;
            for (auto& window : windows) {
                if (window.getWorkspaceID() == work && window.getDrawable() > 0) {
                    setFocusedWindow(window.getDrawable());

                    Debug::log(LOG, "Queueing the warp");

                    const auto PMONITORFROMCURSOR = getMonitorFromCursor();

                    if (PMONITORFROMCURSOR)
                        Debug::log(LOG, "Monitor from cursor: " + std::to_string(PMONITORFROMCURSOR->ID));

                    if (PMONITORFROMCURSOR && PMONITORFROMCURSOR->ID != PMONITOR->ID)
                        QueuedPointerWarp = Vector2D(window.getPosition() + window.getSize() / 2.f);

                    shouldHopToScreen = false;
                    Debug::log(LOG, "No hopping to new workspace");
                    break;
                }
            }

            Debug::log(LOG, "Queueing a hop if needed.");

            if (shouldHopToScreen)
                QueuedPointerWarp = Vector2D(PMONITOR->vecPosition + PMONITOR->vecSize / 2.f);
        } else {
            const auto PFULLWINDOW = getFullscreenWindowByWorkspace(work);
            if (PFULLWINDOW) {
                Debug::log(LOG, "Fullscreen window id " + std::to_string(PFULLWINDOW->getDrawable()));
                setFocusedWindow(PFULLWINDOW->getDrawable());
                
                if (getMonitorFromCursor() && getMonitorFromCursor()->ID != PMONITOR->ID)
                    QueuedPointerWarp = Vector2D(PFULLWINDOW->getPosition() + PFULLWINDOW->getSize() / 2.f);
            }
        }
    }
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

    if (workspaceID == SCRATCHPAD_ID)
        return scratchpadActive;

    for (auto& workspace : activeWorkspaces) {
        if (workspace == workspaceID)
            return true;
    }

    return false;
}

void CWindowManager::updateBarInfo() {

    // IPC

    // What we need to send:
    // - Workspace data
    // - Active Workspace

    // If bar disabled, ignore
    if (ConfigManager::getInt("bar:enabled") == 0)
        return;

    SIPCMessageMainToBar message;

    auto PMONITOR = getMonitorFromCursor();
    if (!PMONITOR) {
        Debug::log(ERR, "Monitor was null! (updateBarInfo) Using 0.");
        PMONITOR = &monitors[0];

        if (monitors.size() == 0) {
            Debug::log(ERR, "Not continuing. Monitors size 0.");
            return;
        }
    }

    message.activeWorkspace = activeWorkspaces[PMONITOR->ID];

    auto winname = getWindowFromDrawable(LastWindow) ? getWindowFromDrawable(LastWindow)->getName() : "";
    auto winclassname = getWindowFromDrawable(LastWindow) ? getWindowFromDrawable(LastWindow)->getClassName() : "";

    for (auto& c : winname) {
        // Remove illegal chars
        if (c == '=' || c == '\t')
            c = ' ';
    }

    for (auto& c : winclassname) {
        // Remove illegal chars
        if (c == '=' || c == '\t')
            c = ' ';
    }

    message.lastWindowName = winname;

    message.lastWindowClass = winclassname;

    auto* const WORKSPACE = getWorkspaceByID(activeWorkspaces[ConfigManager::getInt("bar:monitor") > monitors.size() ? 0 : ConfigManager::getInt("bar:monitor")]);
    if (WORKSPACE)
        message.fullscreenOnBar = WORKSPACE->getHasFullscreenWindow();
    else
        message.fullscreenOnBar = false;

    for (auto& workspace : workspaces) {
        if (workspace.getID() == SCRATCHPAD_ID)
            continue;

        message.openWorkspaces.push_back(workspace.getID());
    }

    IPCSendMessage(m_sIPCBarPipeIn.szPipeName, message);


    // Also check if the bar should be made invisibel
    // we make it by moving it far far away
    // the bar will also stop all updates
    if (message.fullscreenOnBar) {
        if (lastKnownBarPosition.x == -1 && lastKnownBarPosition.y == -1) {
            lastKnownBarPosition = monitors[ConfigManager::getInt("bar:monitor") > monitors.size() ? 0 : ConfigManager::getInt("bar:monitor")].vecPosition;
        }

        Values[0] = (int)-99999;
        Values[1] = (int)-99999;
        xcb_configure_window(DisplayConnection, barWindowID, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
    } else {
        if (lastKnownBarPosition.x != -1 && lastKnownBarPosition.y != -1) {
            Values[0] = (int)lastKnownBarPosition.x;
            Values[1] = (int)lastKnownBarPosition.y;
            xcb_configure_window(DisplayConnection, barWindowID, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);
        }

        lastKnownBarPosition = Vector2D(-1, -1);
    }
}

void CWindowManager::setAllFloatingWindowsTop() {
    for (auto& window : windows) {
        if (window.getIsFloating() && !window.getTransient()) {
            Values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(g_pWindowManager->DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_STACK_MODE, Values);

            window.bringTopRecursiveTransients();
        } else {
            window.bringTopRecursiveTransients();
        }
    }

    // set the bar topper jic
    Values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_pWindowManager->DisplayConnection, barWindowID, XCB_CONFIG_WINDOW_STACK_MODE, Values);
}

void CWindowManager::setAWindowTop(xcb_window_t window) {
    Values[0] = XCB_STACK_MODE_ABOVE;
    const auto COOKIE = xcb_configure_window(g_pWindowManager->DisplayConnection, window, XCB_CONFIG_WINDOW_STACK_MODE, Values);
    Events::ignoredEvents.push_back(COOKIE.sequence);

    // set the bar topper jic
    Values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_pWindowManager->DisplayConnection, barWindowID, XCB_CONFIG_WINDOW_STACK_MODE, Values);
}

bool CWindowManager::shouldBeFloatedOnInit(int64_t window) {
    // Should be floated also sets some properties

    const auto PWINDOW = getWindowFromDrawable(window);

    if (!PWINDOW) {
        Debug::log(ERR, "shouldBeFloatedOnInit with an invalid window!");
        return true;
    }
        
    
    const auto WINCLASS = getClassName(window);
    const auto CLASSNAME = WINCLASS.second;
    const auto CLASSINSTANCE = WINCLASS.first;

    Debug::log(LOG, "New window got class " + (std::string)CLASSINSTANCE + " -> " + CLASSNAME);

    xcb_change_property(DisplayConnection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen("hypr"), "hypr");

    // Role stuff
    const auto WINROLE = getRoleName(window);

    Debug::log(LOG, "Window opened with a role of " + WINROLE);

    // Set it in the pwindow
    PWINDOW->setClassName(CLASSNAME);
    PWINDOW->setRoleName(WINROLE);

    //
    // Type stuff
    //
    PROP(wm_type_cookie, HYPRATOMS["_NET_WM_WINDOW_TYPE"], UINT32_MAX);

    if (wm_type_cookiereply == NULL || xcb_get_property_value_length(wm_type_cookiereply) < 1) {
        Debug::log(LOG, "No preferred type found. (shouldBeFloatedOnInit)");
    } else {
        const auto ATOMS = (xcb_atom_t*)xcb_get_property_value(wm_type_cookiereply);
        if (!ATOMS) {
            Debug::log(ERR, "Atoms not found in preferred type!");
        } else {
            if (xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"])) {
                free(wm_type_cookiereply);
                return true;
            } else if (xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"])
                || xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"])
                || xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"])
                || xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_STATE_MODAL"])
                || xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"])) {
                
                Events::nextWindowCentered = true;
                free(wm_type_cookiereply);
                return true;
            }
        }
    }
    free(wm_type_cookiereply);
    //
    //
    //

    // Verify the rules.
    for (auto& rule : ConfigManager::getMatchingRules(window)) {
        if (rule.szRule == "tile")
            return false;
        else if (rule.szRule == "float")
            return true;
        else if (rule.szRule == "nointerventions") {
            PWINDOW->setNoInterventions(true);
            PWINDOW->setImmovable(true);
            return true;
        }
    }

    return false;
}

void CWindowManager::updateActiveWindowName() {
    if (!getWindowFromDrawable(LastWindow))
        return;

    const auto PLASTWINDOW = getWindowFromDrawable(LastWindow);

    auto WINNAME = getWindowName(LastWindow);
    if (WINNAME != PLASTWINDOW->getName()) {
        Debug::log(LOG, "Update, window got name: " + WINNAME);
        PLASTWINDOW->setName(WINNAME);
    }
}

void CWindowManager::doPostCreationChecks(CWindow* pWindow) {
    //
    Debug::log(LOG, "Post creation checks init");

    const auto window = pWindow->getDrawable();

    PROP(wm_type_cookie, HYPRATOMS["_NET_WM_WINDOW_TYPE"], UINT32_MAX);

    if (wm_type_cookiereply == NULL || xcb_get_property_value_length(wm_type_cookiereply) < 1) {
        Debug::log(LOG, "No preferred type found. (doPostCreationChecks)");
    } else {
        const auto ATOMS = (xcb_atom_t*)xcb_get_property_value(wm_type_cookiereply);
        if (!ATOMS) {
            Debug::log(ERR, "Atoms not found in preferred type!");
        } else {
            if (xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_STATE_FULLSCREEN"])) {
                // set it fullscreen
                pWindow->setFullscreen(true);

                setFocusedWindow(window);
                
                KeybindManager::toggleActiveWindowFullscreen("");
            }
        }
    }
    free(wm_type_cookiereply);

    // Check if it has a name
    const auto NAME = getClassName(window);
    if (NAME.first == "Error" && NAME.second == "Error") {
        Debug::log(WARN, "Window created but has a class of NULL?");
    }

    Debug::log(LOG, "Post creation checks ended");
    //
}

void CWindowManager::getICCCMWMProtocols(CWindow* pWindow) {
    xcb_icccm_get_wm_protocols_reply_t WMProtocolsReply;
    if (!xcb_icccm_get_wm_protocols_reply(DisplayConnection,
        xcb_icccm_get_wm_protocols(DisplayConnection, pWindow->getDrawable(), HYPRATOMS["WM_PROTOCOLS"]), &WMProtocolsReply, NULL))
        return;

    for (auto i = 0; i < (int)WMProtocolsReply.atoms_len; i++) {
        if (WMProtocolsReply.atoms[i] == HYPRATOMS["WM_DELETE_WINDOW"])
            pWindow->setCanKill(true);
    }
    
    xcb_icccm_get_wm_protocols_reply_wipe(&WMProtocolsReply);
}

void CWindowManager::refocusWindowOnClosed() {
    const auto PWINDOW = findWindowAtCursor();

    // No window or last window valid
    if (!PWINDOW || getWindowFromDrawable(LastWindow)) {
        setFocusedWindow(Screen->root);  //refocus on root
            
        return;
    }

    LastWindow = PWINDOW->getDrawable();

    setFocusedWindow(PWINDOW->getDrawable());
}

void CWindowManager::recalcAllWorkspaces() {
    for (auto& workspace : workspaces) {
        recalcEntireWorkspace(workspace.getID());
    }
}

void CWindowManager::moveWindowToUnmapped(int64_t id) {
    if (ConfigManager::getInt("no_unmap_saving") == 1){
        closeWindowAllChecks(id);
        return;
    }

    for (auto& w : windows) {
        if (w.getDrawable() == id) {
            // Move it
            unmappedWindows.push_back(w);
            removeWindowFromVectorSafe(w.getDrawable());
            return;
        }
    }
}

void CWindowManager::moveWindowToMapped(int64_t id) {
    for (auto& w : unmappedWindows) {
        if (w.getDrawable() == id) {
            // Move it
            windows.push_back(w);
            // manually remove
            auto temp = unmappedWindows;
            unmappedWindows.clear();

            for (auto& t : temp) {
                if (t.getDrawable() != id)
                    unmappedWindows.push_back(t);
            }

            windows[windows.size() - 1].setUnderFullscreen(false);
            windows[windows.size() - 1].setDirty(true);
            windows[windows.size() - 1].setLastUpdatePosition(Vector2D(0,0));
            windows[windows.size() - 1].setLastUpdateSize(Vector2D(0,0));

            return;
        }
    }
}

bool CWindowManager::isWindowUnmapped(int64_t id) {
    for (auto& w : unmappedWindows) {
        if (w.getDrawable() == id) {
            return true;
        }
    }

    return false;
}

void CWindowManager::setAllWorkspaceWindowsAboveFullscreen(const int& workspace) {
    for (auto& w : windows) {
        if (w.getWorkspaceID() == workspace && w.getIsFloating()) {
            w.setUnderFullscreen(false);
        }
    }
}

void CWindowManager::setAllWorkspaceWindowsUnderFullscreen(const int& workspace) {
    for (auto& w : windows) {
        if (w.getWorkspaceID() == workspace && w.getIsFloating()) {
            w.setUnderFullscreen(true);
        }
    }
}

void CWindowManager::toggleWindowFullscrenn(const int& window) {
    const auto PWINDOW = getWindowFromDrawable(window);

    if (!PWINDOW)
        return;

    const auto MONITOR = getMonitorFromWindow(PWINDOW);

    if (getWorkspaceByID(activeWorkspaces[MONITOR->ID])->getHasFullscreenWindow() && !PWINDOW->getFullscreen()) {
        Debug::log(LOG, "Not making a window fullscreen because there already is one!");
        return;
    }

    setAllWorkspaceWindowsDirtyByID(activeWorkspaces[MONITOR->ID]);

    PWINDOW->setFullscreen(!PWINDOW->getFullscreen());
    getWorkspaceByID(PWINDOW->getWorkspaceID())->setHasFullscreenWindow(PWINDOW->getFullscreen());

    // Fix windows over and below fullscreen.
    if (PWINDOW->getFullscreen())
        setAllWorkspaceWindowsUnderFullscreen(activeWorkspaces[MONITOR->ID]);
    else
        setAllWorkspaceWindowsAboveFullscreen(activeWorkspaces[MONITOR->ID]);

    // EWMH 
    Values[0] = HYPRATOMS["_NET_WM_STATE_FULLSCREEN"];
    if (PWINDOW->getFullscreen())
        xcb_change_property(DisplayConnection, XCB_PROP_MODE_APPEND, window, HYPRATOMS["_NET_WM_STATE"], XCB_ATOM_ATOM, 32, 1, Values);
    else
        removeAtom(window, HYPRATOMS["_NET_WM_STATE"], HYPRATOMS["_NET_WM_STATE_FULLSCREEN"]);

    EWMH::updateWindow(window);

    Debug::log(LOG, "Set fullscreen to " + std::to_string(PWINDOW->getFullscreen()) + " for " + std::to_string(window));
}

void CWindowManager::handleClientMessage(xcb_client_message_event_t* E) {

    const auto PWINDOW = getWindowFromDrawable(E->window);

    if (E->type == HYPRATOMS["_NET_WM_STATE"]) {
        // The window wants to change its' state.
        // For now we only support FULLSCREEN

        if (!PWINDOW){
            Debug::log(ERR, "Requested _NET_WM_STATE with an invalid window ID! Ignoring.");
            return;
        }

        if (E->data.data32[1] == HYPRATOMS["_NET_WM_STATE_FULLSCREEN"]) {
            if ((PWINDOW->getFullscreen() && (E->data.data32[0] == 0 || E->data.data32[0] == 2))
                || (!PWINDOW->getFullscreen() && (E->data.data32[0] == 1 || E->data.data32[0] == 2))) {

                // Toggle fullscreen
                toggleWindowFullscrenn(PWINDOW->getDrawable());
            }

            Debug::log(LOG, "Message recieved to toggle fullscreen for " + std::to_string(PWINDOW->getDrawable()));
        }
    } else if (E->type == HYPRATOMS["_NET_ACTIVE_WINDOW"]) {
        // Change the focused window
        if (E->format != 32)
            return;

        if (!PWINDOW) {
            Debug::log(ERR, "Requested _NET_ACTIVE_WINDOW with an invalid window ID! Ignoring.");
            return;
        }

        Debug::log(LOG, "Request to change active window to " + std::to_string(PWINDOW->getDrawable()));

        setFocusedWindow(PWINDOW->getDrawable());

        Debug::log(LOG, "Message recieved to set active for " + std::to_string(PWINDOW->getDrawable()));
    } else if (E->type == HYPRATOMS["_NET_MOVERESIZE_WINDOW"]) {
        void *const PEVENT = calloc(32, 1);
        xcb_configure_request_event_t* const GENEV = (xcb_configure_request_event_t*)PEVENT;

        GENEV->window = E->window;
        GENEV->response_type = XCB_CONFIGURE_REQUEST;

        GENEV->value_mask = 0;
        if (E->data.data32[0] & _NET_MOVERESIZE_WINDOW_X) {
            GENEV->value_mask |= XCB_CONFIG_WINDOW_X;
            GENEV->x = E->data.data32[1];
        }
        if (E->data.data32[0] & _NET_MOVERESIZE_WINDOW_Y) {
            GENEV->value_mask |= XCB_CONFIG_WINDOW_Y;
            GENEV->y = E->data.data32[2];
        }
        if (E->data.data32[0] & _NET_MOVERESIZE_WINDOW_WIDTH) {
            GENEV->value_mask |= XCB_CONFIG_WINDOW_WIDTH;
            GENEV->width = E->data.data32[3];
        }
        if (E->data.data32[0] & _NET_MOVERESIZE_WINDOW_HEIGHT) {
            GENEV->value_mask |= XCB_CONFIG_WINDOW_HEIGHT;
            GENEV->height = E->data.data32[4];
        }

        Events::eventConfigure((xcb_generic_event_t*)GENEV);
        free(GENEV);
    } else if (E->type == HYPRATOMS["_NET_CURRENT_DESKTOP"]) {
        // request to change the workspace to something else
        // likely a bar
        // emitted by xcb_ewmh_request_change_current_desktop

        const auto WORK = E->data.data32[0] + 1; // +1 because our first ID is 1 and ewmh's is 0

        Debug::log(LOG, "External request to switch to workspace " + std::to_string(WORK));

        if (!getWorkspaceByID(WORK)) {
            Debug::log(ERR, "Workspace ID " + std::to_string(WORK) + " does NOT exist! Ignoring.");
            return;
        }

        changeWorkspaceByID(WORK);
    }
}

void CWindowManager::recalcAllDocks() {
    for (auto& mon : monitors) {
        mon.vecReservedTopLeft = {0, 0};
        mon.vecReservedBottomRight = {0, 0};

        setAllWorkspaceWindowsDirtyByID(activeWorkspaces[mon.ID]);
    }

    for (auto& w : windows) {
        if (!w.getDock() || w.getDead() || !w.getIsFloating())
            continue;

        const auto MONITOR = &monitors[w.getMonitor()];

        const auto VERTICAL = w.getSize().x / w.getSize().y < 1;

        if (VERTICAL) {
            if (w.getPosition().x < MONITOR->vecSize.x / 2.f + MONITOR->vecPosition.x) {
                // Left
                MONITOR->vecReservedTopLeft = Vector2D(w.getSize().x, 0);
            } else {
                // Right
                MONITOR->vecReservedBottomRight = Vector2D(w.getSize().x, 0);
            }
        } else {
            if (w.getPosition().y < MONITOR->vecSize.y / 2.f + MONITOR->vecPosition.y) {
                // Top
                MONITOR->vecReservedTopLeft = Vector2D(0, w.getSize().y);
            } else {
                // Bottom
                MONITOR->vecReservedBottomRight = Vector2D(0, w.getSize().y);
            }
        }

        // Move it
        Values[0] = w.getDefaultPosition().x;
        Values[1] = w.getDefaultPosition().y;
        xcb_configure_window(DisplayConnection, w.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

        Values[0] = w.getDefaultSize().x;
        Values[1] = w.getDefaultSize().y;
        xcb_configure_window(DisplayConnection, w.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
    }
}

void CWindowManager::startWipeAnimOnWorkspace(const int& oldwork, const int& newwork) {
    const auto PMONITOR = getMonitorFromWorkspace(newwork);

    for (auto& work : workspaces) {
        if (work.getID() == oldwork) {
            if (ConfigManager::getInt("animations:workspaces") == 1)
                work.setCurrentOffset(Vector2D(0,0));
            else
                work.setCurrentOffset(Vector2D(150000, 150000));
            work.setGoalOffset(Vector2D(PMONITOR->vecSize.x, 0));
            work.setAnimationInProgress(true);
        } else if (work.getID() == newwork) {
            if (ConfigManager::getInt("animations:workspaces") == 1)
                work.setCurrentOffset(Vector2D(-PMONITOR->vecSize.x, 0));
            else
                work.setCurrentOffset(Vector2D(0, 0));
            work.setGoalOffset(Vector2D(0, 0));
            work.setAnimationInProgress(true);
        }
    }
}

void CWindowManager::dispatchQueuedWarp() {
    if (QueuedPointerWarp.x == -1 && QueuedPointerWarp.y == -1)
        return;

    warpCursorTo(QueuedPointerWarp);
    QueuedPointerWarp = Vector2D(-1,-1);
}

bool CWindowManager::shouldBeManaged(const int& window) {
    const auto WINDOWATTRS = xcb_get_window_attributes_reply(DisplayConnection, xcb_get_window_attributes(DisplayConnection, window), NULL);

    if (!WINDOWATTRS) {
        Debug::log(LOG, "Skipping: window attributes null");
        return false;
    }

    if (WINDOWATTRS->override_redirect) {
        Debug::log(LOG, "Skipping: override redirect");
        return false;
    }

    const auto GEOMETRY = xcb_get_geometry_reply(DisplayConnection, xcb_get_geometry(DisplayConnection, window), NULL);
    if (!GEOMETRY) {
        Debug::log(LOG, "Skipping: No geometry");
        return false;
    }

    Debug::log(LOG, "shouldBeManaged passed!");

    return true;
}

SMonitor* CWindowManager::getMonitorFromCoord(const Vector2D coord) {
    for (auto& m : monitors) {
        if (VECINRECT(coord, m.vecPosition.x, m.vecPosition.y, m.vecPosition.x + m.vecSize.x, m.vecPosition.y + m.vecSize.y))
            return &m;
    }

    return nullptr;
}

void CWindowManager::changeSplitRatioCurrent(const char& dir) {

    const auto CURRENT = getWindowFromDrawable(LastWindow);

    if (!CURRENT) {
        Debug::log(LOG, "Cannot change split ratio when lastwindow NULL.");
        return;
    }

    const auto PARENT = getWindowFromDrawable(CURRENT->getParentNodeID());

    if (!PARENT) {
        Debug::log(LOG, "Cannot change split ratio when parent NULL.");
        return;
    }

    switch(dir) {
        case '+':
            PARENT->setSplitRatio(PARENT->getSplitRatio() + 0.05f);
            break;
        case '-':
            PARENT->setSplitRatio(PARENT->getSplitRatio() - 0.05f);
            break;
        default:
            Debug::log(ERR, "changeSplitRatioCurrent called with an invalid dir!");
            return;
    }

    PARENT->setSplitRatio(std::clamp(PARENT->getSplitRatio(), 0.1f, 1.9f));

    Debug::log(LOG, "Changed SplitRatio of " + std::to_string(PARENT->getDrawable()) + " to " + std::to_string(PARENT->getSplitRatio()) + " (" + dir + ")" );

    recalcEntireWorkspace(CURRENT->getWorkspaceID());
}

void CWindowManager::getICCCMSizeHints(CWindow* pWindow) {
    xcb_size_hints_t sizeHints;
    const auto succ = xcb_icccm_get_wm_normal_hints_reply(g_pWindowManager->DisplayConnection, xcb_icccm_get_wm_normal_hints_unchecked(g_pWindowManager->DisplayConnection, pWindow->getDrawable()), &sizeHints, NULL);
    
    if (succ) {
        auto NEWSIZE = Vector2D(std::max(std::max(sizeHints.width, (int32_t)pWindow->getDefaultSize().x), std::max(sizeHints.max_width > g_pWindowManager->monitors[pWindow->getMonitor()].vecSize.x ? 0 : sizeHints.max_width, sizeHints.base_width)),
                                std::max(std::max(sizeHints.height, (int32_t)pWindow->getDefaultSize().y), std::max(sizeHints.max_height > g_pWindowManager->monitors[pWindow->getMonitor()].vecSize.y ? 0 : sizeHints.max_height, sizeHints.base_height)));

        pWindow->setPseudoSize(NEWSIZE);
    } else {
        Debug::log(ERR, "ICCCM Size Hints failed.");
    }
}

void CWindowManager::processCursorDeltaOnWindowResizeTiled(CWindow* pWindow, const Vector2D& pointerDelta) {
    // this resizes the window based on cursor movement,
    // basically like a mouse-ver of splitratio

    if (!pWindow)
        return;

    // TODO: support master-stack
    if (ConfigManager::getInt("layout") == LAYOUT_MASTER){
        Debug::log(WARN, "processCursorDeltaOnWindowResizeTiled does NOT support MASTER yet. Ignoring.");
        return;
    }

    // Construct an allowed delta movement
    const auto PMONITOR             = getMonitorFromWindow(pWindow);
    const bool DISPLAYLEFT          = STICKS(pWindow->getPosition().x, PMONITOR->vecPosition.x);
    const bool DISPLAYRIGHT         = STICKS(pWindow->getPosition().x + pWindow->getSize().x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x);
    const bool DISPLAYTOP           = STICKS(pWindow->getPosition().y, PMONITOR->vecPosition.y);
    const bool DISPLAYBOTTOM        = STICKS(pWindow->getPosition().y + pWindow->getSize().y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y);

    Vector2D allowedMovement = pointerDelta;
    if (DISPLAYLEFT && DISPLAYRIGHT)
        allowedMovement.x = 0;

    if (DISPLAYTOP && DISPLAYBOTTOM)
        allowedMovement.y = 0;

    // Get the correct containers to apply the splitratio to
    const auto PPARENT = getWindowFromDrawable(pWindow->getParentNodeID());

    // If there is no parent we ignore the request (only window)
    if (!PPARENT)
        return;

    const bool PARENTSIDEBYSIDE = PPARENT->getSize().x / PPARENT->getSize().y > 1;

    // Get the parent's parent.
    const auto PPARENT2 = getWindowFromDrawable(PPARENT->getParentNodeID());

    // if there is no parent, we have 2 windows only and have the ability to drag in only one direction.
    if (!PPARENT2) {
        if (PARENTSIDEBYSIDE) {
            // splitratio adjust for pixels
            allowedMovement.x *= 2.f / PPARENT->getSize().x;
            PPARENT->setSplitRatio(std::clamp(PPARENT->getSplitRatio() + allowedMovement.x, (double)0.05f, (double)1.95f));
            PPARENT->recalcSizePosRecursive();
        } else {
            allowedMovement.y *= 2.f / PPARENT->getSize().y;
            PPARENT->setSplitRatio(std::clamp(PPARENT->getSplitRatio() + allowedMovement.y, (double)0.05f, (double)1.95f));
            PPARENT->recalcSizePosRecursive();
        }

        return;
    }

    // if there is a parent, we have 2 axes of freedom
    const auto SIDECONTAINER = PARENTSIDEBYSIDE ? PPARENT : PPARENT2;
    const auto TOPCONTAINER = PARENTSIDEBYSIDE ? PPARENT2 : PPARENT;

    allowedMovement.x *= 2.f / SIDECONTAINER->getSize().x;
    allowedMovement.y *= 2.f / TOPCONTAINER->getSize().x;

    SIDECONTAINER->setSplitRatio(std::clamp(SIDECONTAINER->getSplitRatio() + allowedMovement.x, (double)0.05f, (double)1.95f));
    TOPCONTAINER->setSplitRatio(std::clamp(TOPCONTAINER->getSplitRatio() + allowedMovement.y, (double)0.05f, (double)1.95f));
    SIDECONTAINER->recalcSizePosRecursive();
    TOPCONTAINER->recalcSizePosRecursive();
}