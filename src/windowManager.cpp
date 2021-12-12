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

    const auto EXTENSIONREPLY = xcb_get_extension_data(DisplayConnection, &xcb_randr_id);
    if (!EXTENSIONREPLY->present)
        Debug::log(ERR, "RandR extension missing");
    else {
        //listen for screen change events
        xcb_randr_select_input(DisplayConnection, Screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    }

    xcb_flush(DisplayConnection);

    if (monitors.size() == 0) {
        // RandR failed!
        Debug::log(WARN, "RandR failed!");
        monitors.clear();

        #define TESTING_MON_AMOUNT 3
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
    setupRandrMonitors();

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

    // ---- INIT THE THREAD FOR ANIM & CONFIG ---- //

    // start its' update thread
    Events::setThread();

    Debug::log(LOG, "Thread (Parent) done.");

    ConfigManager::loadConfigLoadVars();

    updateRootCursor();

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

    // remove unused workspaces
    cleanupUnusedWorkspaces();

    // Update last window name
    updateActiveWindowName();

    // Update the bar with the freshest stuff
    updateBarInfo();

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

        // Set thread state, halt animations until done.
        mainThreadBusy = true;

        // Read from the bar
        if (!g_pWindowManager->statusBar)
            IPCRecieveMessageM(m_sIPCBarPipeOut.szPipeName);

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

            default:

                if ((EVENTCODE != 14) && (EVENTCODE != 13) && (EVENTCODE != 0) && (EVENTCODE != 22))
                    Debug::log(WARN, "Unknown event: " + std::to_string(ev->response_type & ~0x80));
                break;
        }

        free(ev);
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
            window.setDirty(false);

            // Check if the window isn't a node or has the noInterventions prop
            if (window.getChildNodeAID() != 0 || window.getNoInterventions()) 
                continue;
                
            setEffectiveSizePosUsingConfig(&window);

            // Fullscreen flag
            bool bHasFullscreenWindow = getWorkspaceByID(window.getWorkspaceID())->getHasFullscreenWindow();

            // first and foremost, let's check if the window isn't on a hidden workspace
            // or that it is not a non-fullscreen window in a fullscreen workspace thats under
            if (!isWorkspaceVisible(window.getWorkspaceID())
                || (bHasFullscreenWindow && !window.getFullscreen() && (window.getUnderFullscreen() || !window.getIsFloating()))) {
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

                const auto MONITOR = getMonitorFromWindow(&window);

                Values[0] = (int)MONITOR->vecSize.x;
                Values[1] = (int)MONITOR->vecSize.y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);

                Values[0] = (int)MONITOR->vecPosition.x;
                Values[1] = (int)MONITOR->vecPosition.y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

                // Apply rounded corners, does all the checks inside
                applyShapeToWindow(&window);

                continue;
            }

            // Update the position because the border makes the window jump
            // I have added the bordersize vec2d before in the setEffectiveSizePosUsingConfig function.
            Values[0] = (int)window.getRealPosition().x - ConfigManager::getInt("border_size");
            Values[1] = (int)window.getRealPosition().y - ConfigManager::getInt("border_size");
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, Values);

            Values[0] = (int)ConfigManager::getInt("border_size");
            xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_BORDER_WIDTH, Values);


            // do border
            Values[0] = window.getRealBorderColor().getAsUint32();
            xcb_change_window_attributes(DisplayConnection, window.getDrawable(), XCB_CW_BORDER_PIXEL, Values);

            // If it isn't animated or we have non-cheap animations, update the real size
            if (!window.getIsAnimated() || ConfigManager::getInt("anim:cheap") == 0) {
                Values[0] = (int)window.getRealSize().x;
                Values[1] = (int)window.getRealSize().y;
                xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
                window.setFirstAnimFrame(true);
            }

            if (ConfigManager::getInt("anim:cheap") == 1 && window.getFirstAnimFrame() && window.getIsAnimated()) {
                // first frame, fix the size if smaller
                window.setFirstAnimFrame(false);
                if (window.getRealSize().x < window.getEffectiveSize().x || window.getRealSize().y < window.getEffectiveSize().y) {
                    Values[0] = (int)window.getEffectiveSize().x;
                    Values[1] = (int)window.getEffectiveSize().y;
                    xcb_configure_window(DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, Values);
                }
            }

            applyShapeToWindow(&window);
        }
    }

    Debug::log(LOG, "Refreshed dirty windows.");
}

void CWindowManager::setFocusedWindow(xcb_drawable_t window) {
    if (window && window != Screen->root) {
        // border color
        if (const auto PLASTWIN = getWindowFromDrawable(LastWindow); PLASTWIN) {
            PLASTWIN->setEffectiveBorderColor(CFloatingColor(ConfigManager::getInt("col.inactive_border")));
        }
        if (const auto PLASTWIN = getWindowFromDrawable(window); PLASTWIN) {
            PLASTWIN->setEffectiveBorderColor(CFloatingColor(ConfigManager::getInt("col.active_border")));
        }

        float values[1];
        if (g_pWindowManager->getWindowFromDrawable(window) && g_pWindowManager->getWindowFromDrawable(window)->getIsFloating()) {
            values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(g_pWindowManager->DisplayConnection, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
        }

        // Apply rounded corners, does all the checks inside.
        // The border changed so let's not make it rectangular maybe
        applyShapeToWindow(g_pWindowManager->getWindowFromDrawable(window));

        LastWindow = window;

        applyShapeToWindow(g_pWindowManager->getWindowFromDrawable(window));

        // set focus in X11
        xcb_set_input_focus(DisplayConnection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
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
                }
            }

            // Check #3: Check if the window exists with xcb
            // Some windows do not report they are dead for w/e reason
            if (w.getDrawable() > 0) {
                const auto GEOMETRYCOOKIE = xcb_get_geometry(g_pWindowManager->DisplayConnection, w.getDrawable());
                const auto GEOMETRY = xcb_get_geometry_reply(g_pWindowManager->DisplayConnection, GEOMETRYCOOKIE, 0);

                if (!GEOMETRY || (GEOMETRY->width < 1 || GEOMETRY->height < 1)) {
                    Debug::log(LOG, "Found a dead window, ID: " + std::to_string(w.getDrawable()) + ", removing it.");
                    
                    closeWindowAllChecks(w.getDrawable());
                    continue;
                }

                // Type 2: is hidden.
                const auto window = w.getDrawable();
                PROP(wm_type_cookie, HYPRATOMS["_NET_WM_WINDOW_TYPE"], UINT32_MAX);

                if (wm_type_cookiereply == NULL || xcb_get_property_value_length(wm_type_cookiereply) < 1) {
                    Debug::log(LOG, "No preferred type found.");
                } else {
                    const auto ATOMS = (xcb_atom_t*)xcb_get_property_value(wm_type_cookiereply);
                    if (!ATOMS) {
                        Debug::log(ERR, "Atoms not found in preferred type!");
                    } else {
                        if (xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_STATE_HIDDEN"])) {
                            // delete it
                            // NOTE: this is NOT the cause of windows in tray not being able
                            // to open.
                            free(wm_type_cookiereply);
                            
                            Debug::log(LOG, "Found a dead window, ID: " + std::to_string(w.getDrawable()) + ", removing it.");

                            closeWindowAllChecks(w.getDrawable());
                            continue;
                        }
                    }
                }
                free(wm_type_cookiereply);
            }
        }
    }
}

CWindow* CWindowManager::getWindowFromDrawable(int64_t window) {
    if (!window)
        return nullptr;

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

void CWindowManager::applyShapeToWindow(CWindow* pWindow) {
    if (!pWindow)
        return;

    const auto ROUNDING = pWindow->getFullscreen() ? 0 : ConfigManager::getInt("rounding");

    const auto SHAPEQUERY = xcb_get_extension_data(DisplayConnection, &xcb_shape_id);

    if (!SHAPEQUERY || !SHAPEQUERY->present || pWindow->getNoInterventions())
        return;

    // Prepare values

    const auto MONITOR = getMonitorFromWindow(pWindow);
    const uint16_t W = pWindow->getFullscreen() ? MONITOR->vecSize.x : pWindow->getRealSize().x;
    const uint16_t H = pWindow->getFullscreen() ? MONITOR->vecSize.y : pWindow->getRealSize().y;
    const uint16_t BORDER = pWindow->getFullscreen() ? 0 : ConfigManager::getInt("border_size");
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
    const auto BARHEIGHT = ConfigManager::getInt("bar:enabled") == 1 ? ConfigManager::getInt("bar:height") : ConfigManager::parseError == "" ? 0 : ConfigManager::getInt("bar:height");

    // set some flags.
    const bool DISPLAYLEFT          = STICKS(pWindow->getPosition().x, MONITOR->vecPosition.x);
    const bool DISPLAYRIGHT         = STICKS(pWindow->getPosition().x + pWindow->getSize().x, MONITOR->vecPosition.x + MONITOR->vecSize.x);
    const bool DISPLAYTOP           = STICKS(pWindow->getPosition().y, MONITOR->vecPosition.y);
    const bool DISPLAYBOTTOM        = STICKS(pWindow->getPosition().y + pWindow->getSize().y, MONITOR->vecPosition.y + MONITOR->vecSize.y);

    pWindow->setEffectivePosition(pWindow->getPosition() + Vector2D(ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size")));
    pWindow->setEffectiveSize(pWindow->getSize() - (Vector2D(ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size")) * 2));

    // do gaps, set top left
    pWindow->setEffectivePosition(pWindow->getEffectivePosition() + Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID == ConfigManager::getInt("bar:monitor") ? BARHEIGHT : 0) : ConfigManager::getInt("gaps_in")));
    // fix to old size bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYLEFT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYTOP ? ConfigManager::getInt("gaps_out") + (MONITOR->ID == ConfigManager::getInt("bar:monitor") ? BARHEIGHT : 0) : ConfigManager::getInt("gaps_in")));
    // set bottom right
    pWindow->setEffectiveSize(pWindow->getEffectiveSize() - Vector2D(DISPLAYRIGHT ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in"), DISPLAYBOTTOM ? ConfigManager::getInt("gaps_out") : ConfigManager::getInt("gaps_in")));
}

CWindow* CWindowManager::findWindowAtCursor() {
    const auto POINTERCOOKIE = xcb_query_pointer(DisplayConnection, Screen->root);

    Vector2D cursorPos = getCursorPos();

    const auto WORKSPACE = activeWorkspaces[getMonitorFromCursor()->ID];

    for (auto& window : windows) {
        if (window.getWorkspaceID() == WORKSPACE && !window.getIsFloating() && window.getDrawable() > 0) {

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

    const auto PMONITOR = getMonitorFromWindow(pWindow);
    if (!PMONITOR) {
        Debug::log(ERR, "Monitor was nullptr! (calculateNewTileSetOldTile)");
        return;
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
        if (w.getWorkspaceID() == workspace && w.getDrawable() > 0) {
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
                if (w.getWorkspaceID() == workspace && w.getMaster() && !w.getDead()) {
                    pMaster = &w;
                    break;
                }
            }

            if (!pMaster) {
                Debug::log(ERR, "No master found on workspace???");
                return;
            }

            // set the xy for master
            pMaster->setPosition(Vector2D(0, 0) + PMONITOR->vecPosition);
            pMaster->setSize(Vector2D(PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y));

            // get children sorted
            std::vector<CWindow*> children;
            for (auto& w : windows) {
                if (w.getWorkspaceID() == workspace && !w.getMaster() && w.getDrawable() > 0 && !w.getDead())
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
                child->setPosition(Vector2D(PMONITOR->vecSize.x / 2, yoff) + PMONITOR->vecPosition);
                child->setSize(Vector2D(PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y / children.size()));

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
                if (w.getWorkspaceID() == workspace && w.getParentNodeID() == 0) {
                    pMasterWindow = &w;
                    break;
                }
            }

            if (!pMasterWindow) {
                return;
            }

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

    if (!pWindow->getNoInterventions()) {
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

    if (const auto WORKSPACE = getWorkspaceByID(CLOSEDWINDOW->getWorkspaceID()); WORKSPACE && CLOSEDWINDOW->getFullscreen())
        WORKSPACE->setHasFullscreenWindow(false);

    if (!CLOSEDWINDOW->getIsFloating())
        g_pWindowManager->fixWindowOnClose(CLOSEDWINDOW);

    // delete off of the arr
    g_pWindowManager->removeWindowFromVectorSafe(id);
}

void CWindowManager::fixMasterWorkspaceOnClosed(CWindow* pWindow) {
    // get children and master
    CWindow* pMaster = nullptr;
    for (auto& w : windows) {
        if (w.getWorkspaceID() == pWindow->getWorkspaceID() && w.getMaster()) {
            pMaster = &w;
            break;
        }
    }

    if (!pMaster) {
        Debug::log(ERR, "No master found on workspace???");
        return;
    }

    // get children sorted
    std::vector<CWindow*> children;
    for (auto& w : windows) {
        if (w.getWorkspaceID() == pWindow->getWorkspaceID() && !w.getMaster() && w.getDrawable() > 0 && w.getDrawable() != pWindow->getDrawable())
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
            case 'u':
                if (STICKS(POSA.y, POSB.y + SIZEB.y))
                    return &w;
                break;
            case 'b':
            case 'd':
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

void CWindowManager::moveActiveWindowToWorkspace(int workspace) {

    const auto PWINDOW = getWindowFromDrawable(LastWindow);

    if (!PWINDOW)
        return;

    const auto SAVEDDEFAULTSIZE = PWINDOW->getDefaultSize();
    const auto SAVEDFLOATSTATUS = PWINDOW->getIsFloating();
    const auto SAVEDDRAWABLE    = PWINDOW->getDrawable();

    closeWindowAllChecks(SAVEDDRAWABLE);

    // PWINDOW is dead!

    changeWorkspaceByID(workspace);

    // Find new mon
    int NEWMONITOR = 0;
    for (long unsigned int i = 0; i < activeWorkspaces.size(); ++i) {
        if (activeWorkspaces[i] == workspace) {
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

    if (newLastWindow) {
        setFocusedWindow(newLastWindow);
    }


    CWindow* PNEWWINDOW = nullptr;
    if (SAVEDFLOATSTATUS)
        PNEWWINDOW = Events::remapFloatingWindow(SAVEDDRAWABLE, NEWMONITOR);
    else
        PNEWWINDOW = Events::remapWindow(SAVEDDRAWABLE, false, NEWMONITOR);


    PNEWWINDOW->setDefaultSize(SAVEDDEFAULTSIZE);
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

CWindow* CWindowManager::getFullscreenWindowByWorkspace(const int& id) {
    for (auto& window : windows) {
        if (window.getWorkspaceID() == id && window.getFullscreen())
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
            // set workspaces dirty
            setAllWorkspaceWindowsDirtyByID(activeWorkspaces[workspace.getMonitor()]);
            setAllWorkspaceWindowsDirtyByID(ID);

            activeWorkspaces[workspace.getMonitor()] = workspace.getID();

            // if not fullscreen set the focus to any window on that workspace
            // if fullscreen, set to the fullscreen window
            const auto PWORKSPACE = getWorkspaceByID(ID);
            if (PWORKSPACE) {
                if (!PWORKSPACE->getHasFullscreenWindow()) {
                    for (auto& window : windows) {
                        if (window.getWorkspaceID() == ID && window.getDrawable() > 0) {
                            setFocusedWindow(window.getDrawable());
                            break;
                        }
                    }
                } else {
                    const auto PFULLWINDOW = getFullscreenWindowByWorkspace(ID);
                    if (PFULLWINDOW)
                        setFocusedWindow(PFULLWINDOW->getDrawable());
                }
            }

            // Update bar info
            updateBarInfo();
            
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

    // IPC

    // What we need to send:
    // - Workspace data
    // - Active Workspace

    // If bar disabled, ignore
    if (ConfigManager::getInt("bar:enabled") == 0)
        return;

    SIPCMessageMainToBar message;

    if (!getMonitorFromCursor()) {
        Debug::log(ERR, "Monitor was null! (updateBarInfo)");
        return;
    }

    message.activeWorkspace = activeWorkspaces[getMonitorFromCursor()->ID];

    auto winname = getWindowFromDrawable(LastWindow) ? getWindowFromDrawable(LastWindow)->getName() : "";
    auto winclassname = getWindowFromDrawable(LastWindow) ? getWindowFromDrawable(LastWindow)->getClassName() : "";

    for (auto& c : winname) {
        // Remove illegal chars
        if (c == '=')
            c = ' ';
        else if (c == '\t')
            c = ' ';
    }

    for (auto& c : winclassname) {
        // Remove illegal chars
        if (c == '=')
            c = ' ';
        else if (c == '\t')
            c = ' ';
    }

    message.lastWindowName = winname;

    message.lastWindowClass = winclassname;

    message.fullscreenOnBar = getWorkspaceByID(activeWorkspaces[ConfigManager::getInt("bar:monitor") > monitors.size() ? 0 : ConfigManager::getInt("bar:monitor")])->getHasFullscreenWindow();

    for (auto& workspace : workspaces) {
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
        if (window.getIsFloating()) {
            Values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(g_pWindowManager->DisplayConnection, window.getDrawable(), XCB_CONFIG_WINDOW_STACK_MODE, Values);
        }
    }
}

void CWindowManager::setAWindowTop(xcb_window_t window) {
    Values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_pWindowManager->DisplayConnection, window, XCB_CONFIG_WINDOW_STACK_MODE, Values);
}

bool CWindowManager::shouldBeFloatedOnInit(int64_t window) {
    // Should be floated also sets some properties
    
    // get stuffza

    // floating for krunner
    // TODO: config this
    
    const auto WINCLASS = getClassName(window);
    const auto CLASSNAME = WINCLASS.second;
    const auto CLASSINSTANCE = WINCLASS.first;

    Debug::log(LOG, "New window got class " + (std::string)CLASSINSTANCE + " -> " + CLASSNAME);

    xcb_change_property(DisplayConnection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen("hypr"), "hypr");

    if (((std::string)CLASSNAME).find("krunner") != std::string::npos) {
        return true;
    }


    // Role stuff
    const auto WINROLE = getRoleName(window);

    Debug::log(LOG, "Window opened with a role of " + WINROLE);

    if (WINROLE.find("pop-up") != std::string::npos || WINROLE.find("task_dialog") != std::string::npos) {
        return true;
    }


    //
    // Type stuff
    //
    PROP(wm_type_cookie, HYPRATOMS["_NET_WM_WINDOW_TYPE"], UINT32_MAX);
    xcb_atom_t TYPEATOM = NULL;

    if (wm_type_cookiereply == NULL || xcb_get_property_value_length(wm_type_cookiereply) < 1) {
        Debug::log(LOG, "No preferred type found.");
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
        Debug::log(LOG, "No preferred type found.");
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
    if (!PWINDOW || getWindowFromDrawable(LastWindow))
        return;

    LastWindow = PWINDOW->getDrawable();

    setFocusedWindow(PWINDOW->getDrawable());
}

void CWindowManager::recalcAllWorkspaces() {
    for (auto& workspace : workspaces) {
        recalcEntireWorkspace(workspace.getID());
    }
}

void CWindowManager::moveWindowToUnmapped(int64_t id) {
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