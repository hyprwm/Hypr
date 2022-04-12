#include "events.hpp"

gpointer handle(gpointer data) {
    int lazyUpdateCounter = 0;

    while (1) {
        // update animations. They should be thread-safe.
        AnimationUtil::move();
        //

        // wait for the main thread to be idle
        while (g_pWindowManager->mainThreadBusy) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        // set state to let the main thread know to wait.
        g_pWindowManager->animationUtilBusy = true;

        // Don't spam these
        if (lazyUpdateCounter > 10){
            // Update the active window name
            g_pWindowManager->updateActiveWindowName();

            // Update the bar
            g_pWindowManager->updateBarInfo();

            // check config
            ConfigManager::tick();

            lazyUpdateCounter = 0;
        }

        ++lazyUpdateCounter;

        // restore anim state
        g_pWindowManager->animationUtilBusy = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / ConfigManager::getInt("max_fps")));
    }
}

void Events::setThread() {

    // Start a GTK thread so that Cairo does not complain.

    g_pWindowManager->barThread = g_thread_new("HyprST", handle, nullptr);

    if (!g_pWindowManager->barThread) {
        Debug::log(ERR, "Gthread failed!");
        return;
    }
}

void Events::eventEnter(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_enter_notify_event_t*>(event);

    RETURNIFBAR;

    if (E->mode != XCB_NOTIFY_MODE_NORMAL)
        return;

    if (E->detail == XCB_NOTIFY_DETAIL_INFERIOR)
        return;

    const auto PENTERWINDOW = g_pWindowManager->getWindowFromDrawable(E->event);

    if (!PENTERWINDOW){

        // we entered an unknown window to us. Let's manage it.
        Debug::log(LOG, "Entered an unmanaged window. Trying to manage it!");

        CWindow newEnteredWindow;
        newEnteredWindow.setDrawable(E->event);
        g_pWindowManager->addWindowToVectorSafe(newEnteredWindow);

        CWindow* pNewWindow;
        if (g_pWindowManager->shouldBeFloatedOnInit(E->event)) {
            Debug::log(LOG, "Window SHOULD be floating on start.");
            pNewWindow = remapFloatingWindow(E->event);
        } else {
            Debug::log(LOG, "Window should NOT be floating on start.");
            pNewWindow = remapWindow(E->event);
        }

        if (!pNewWindow) { // oh well. we tried. 
            g_pWindowManager->removeWindowFromVectorSafe(E->event);
            Debug::log(LOG, "Tried to manage, but failed!");
        }

        return;
    }

    // Only when focus_when_hover OR floating OR last window floating
    if (ConfigManager::getInt("focus_when_hover") == 1
        || PENTERWINDOW->getIsFloating()
        || (g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow) && g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow)->getIsFloating()))
            g_pWindowManager->setFocusedWindow(E->event);

    PENTERWINDOW->setDirty(true);

    if (PENTERWINDOW->getIsSleeping()) {
        // Wake it up, fixes some weird shenaningans
        wakeUpEvent(E->event);
        PENTERWINDOW->setIsSleeping(false);
    }
}

void Events::eventLeave(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_leave_notify_event_t*>(event);

    RETURNIFBAR;

    const auto PENTERWINDOW = g_pWindowManager->getWindowFromDrawable(E->event);

    if (!PENTERWINDOW)
        return;

    if (PENTERWINDOW->getIsSleeping()) {
        // Wake it up, fixes some weird shenaningans
        wakeUpEvent(E->event);
        PENTERWINDOW->setIsSleeping(false);
    }
}

void Events::eventDestroy(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_destroy_notify_event_t*>(event);

    // let bar check if it wasnt a tray item
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->ensureTrayClientDead(E->window);

    RETURNIFBAR;

    Debug::log(LOG, "Destroy called on " + std::to_string(E->window));

    g_pWindowManager->closeWindowAllChecks(E->window);

    // refocus on new window
    g_pWindowManager->refocusWindowOnClosed();

    // EWMH
    EWMH::updateClientList();
}

void Events::eventUnmapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_unmap_notify_event_t*>(event);

    // let bar check if it wasnt a tray item
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->ensureTrayClientHidden(E->window, true);

    RETURNIFBAR;

    const auto PCLOSEDWINDOW = g_pWindowManager->getWindowFromDrawable(E->window);

    if (!PCLOSEDWINDOW) {
        Debug::log(LOG, "Unmap called on an invalid window: " + std::to_string(E->window));
        return; // bullshit window?
    }

    Debug::log(LOG, "Unmap called on " + std::to_string(E->window) + " -> " + PCLOSEDWINDOW->getName());

    if (!PCLOSEDWINDOW->getDock())
        g_pWindowManager->closeWindowAllChecks(E->window);

    // refocus on new window
    g_pWindowManager->refocusWindowOnClosed();

    // EWMH
    EWMH::updateClientList();
}

CWindow* Events::remapFloatingWindow(int windowID, int forcemonitor) {
    // The array is not realloc'd in this method we can make this const
    const auto PWINDOWINARR = g_pWindowManager->getWindowFromDrawable(windowID);

    if (!PWINDOWINARR) {
        Debug::log(ERR, "remapFloatingWindow called with an invalid window!");
        return nullptr;
    }

    PWINDOWINARR->setIsFloating(true);
    PWINDOWINARR->setDirty(true);

    auto PMONITOR = g_pWindowManager->getMonitorFromCursor();
    if (!PMONITOR) {
        Debug::log(ERR, "Monitor was null! (remapWindow) Using 0.");
        PMONITOR = &g_pWindowManager->monitors[0];

        if (g_pWindowManager->monitors.size() == 0) {
            Debug::log(ERR, "Not continuing. Monitors size 0.");
            return nullptr;
        }
            
    }

    // Check the monitor rule
    for (auto& rule : ConfigManager::getMatchingRules(windowID)) {
        if (!PWINDOWINARR->getFirstOpen())
            break;

        if (rule.szRule.find("monitor") == 0) {
            try {
                const auto MONITOR = stoi(rule.szRule.substr(rule.szRule.find(" ") + 1));

                Debug::log(LOG, "Rule monitor, applying to window " + std::to_string(windowID));

                if (MONITOR > g_pWindowManager->monitors.size() || MONITOR < 0)
                    forcemonitor = -1;
                else
                    forcemonitor = MONITOR;
            } catch(...) {
                Debug::log(LOG, "Rule monitor failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        }

        if (rule.szRule.find("pseudo") == 0) {
            PWINDOWINARR->setIsPseudotiled(true);
        }

        if (rule.szRule.find("fullscreen") == 0) {
            PWINDOWINARR->setFullscreen(true);
        }
        
        if (rule.szRule.find("workspace") == 0) {
            try {
                const auto WORKSPACE = stoi(rule.szRule.substr(rule.szRule.find(" ") + 1));

                Debug::log(LOG, "Rule workspace, applying to window " + std::to_string(windowID));

                g_pWindowManager->changeWorkspaceByID(WORKSPACE);
                forcemonitor = g_pWindowManager->getWorkspaceByID(WORKSPACE)->getMonitor();
            } catch (...) {
                Debug::log(LOG, "Rule workspace failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        }
    }

    const auto CURRENTSCREEN = forcemonitor != -1 ? forcemonitor : PMONITOR->ID;
    PWINDOWINARR->setWorkspaceID(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
    PWINDOWINARR->setMonitor(CURRENTSCREEN);

    // Window name
    const auto WINNAME = getWindowName(windowID);
    Debug::log(LOG, "New window got name: " + WINNAME);
    PWINDOWINARR->setName(WINNAME);

    const auto WINCLASSNAME = getClassName(windowID);
    Debug::log(LOG, "New window got class: " + WINCLASSNAME.second);
    PWINDOWINARR->setClassName(WINCLASSNAME.second);

    // For all floating windows, get their default size
    const auto GEOMETRYCOOKIE   = xcb_get_geometry(g_pWindowManager->DisplayConnection, windowID);
    const auto GEOMETRY         = xcb_get_geometry_reply(g_pWindowManager->DisplayConnection, GEOMETRYCOOKIE, 0);

    if (GEOMETRY) {
        PWINDOWINARR->setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition);
        PWINDOWINARR->setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));
    } else {
        Debug::log(ERR, "Geometry failed in remap.");

        PWINDOWINARR->setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition);
        PWINDOWINARR->setDefaultSize(Vector2D(g_pWindowManager->Screen->width_in_pixels / 2.f, g_pWindowManager->Screen->height_in_pixels / 2.f));
    }

    if (PWINDOWINARR->getDefaultSize().x < 40 || PWINDOWINARR->getDefaultSize().y < 40) {
        // min size
        PWINDOWINARR->setDefaultSize(Vector2D(std::clamp(PWINDOWINARR->getDefaultSize().x, (double)40, (double)99999),
                                       std::clamp(PWINDOWINARR->getDefaultSize().y, (double)40, (double)99999)));
    }

    if (nextWindowCentered) {
        PWINDOWINARR->setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition + g_pWindowManager->monitors[CURRENTSCREEN].vecSize / 2.f - PWINDOWINARR->getDefaultSize() / 2.f);
    }

    //
    // Dock Checks
    //
    const auto wm_type_cookie = xcb_get_property(g_pWindowManager->DisplayConnection, false, windowID, HYPRATOMS["_NET_WM_WINDOW_TYPE"], XCB_GET_PROPERTY_TYPE_ANY, 0, (4294967295U));
    const auto wm_type_cookiereply = xcb_get_property_reply(g_pWindowManager->DisplayConnection, wm_type_cookie, NULL);
    xcb_atom_t TYPEATOM = NULL;
    if (wm_type_cookiereply == NULL || xcb_get_property_value_length(wm_type_cookiereply) < 1) {
        Debug::log(LOG, "No preferred type found. (RemapFloatingWindow)");
    } else {
        const auto ATOMS = (xcb_atom_t*)xcb_get_property_value(wm_type_cookiereply);
        if (!ATOMS) {
            Debug::log(ERR, "Atoms not found in preferred type!");
        } else {
            if (xcbContainsAtom(wm_type_cookiereply, HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"])) {
                // set to floating and set the immovable and nointerventions flag
                PWINDOWINARR->setImmovable(true);
                PWINDOWINARR->setNoInterventions(true);

                PWINDOWINARR->setDefaultPosition(Vector2D(GEOMETRY->x, GEOMETRY->y));
                PWINDOWINARR->setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));

                PWINDOWINARR->setDockAlign(DOCK_TOP);

                // Check reserved
                const auto STRUTREPLY = xcb_get_property_reply(g_pWindowManager->DisplayConnection, xcb_get_property(g_pWindowManager->DisplayConnection, false, windowID, HYPRATOMS["_NET_WM_STRUT_PARTIAL"], XCB_GET_PROPERTY_TYPE_ANY, 0, (4294967295U)), NULL);

                if (!STRUTREPLY || xcb_get_property_value_length(STRUTREPLY) == 0) {
                    Debug::log(ERR, "Couldn't get strut for dock.");
                } else {
                    const uint32_t* STRUT = (uint32_t*)xcb_get_property_value(STRUTREPLY);

                    if (!STRUT) {
                        Debug::log(ERR, "Couldn't get strut for dock. (2)");
                    } else {
                        // Set the dock's align
                        // LEFT RIGHT TOP BOTTOM
                        //  0     1    2     3
                        if (STRUT[2] > 0 && STRUT[3] == 0) {
                            // top
                            PWINDOWINARR->setDockAlign(DOCK_TOP);
                        } else if (STRUT[2] == 0 && STRUT[3] > 0) {
                            // bottom
                            PWINDOWINARR->setDockAlign(DOCK_BOTTOM);
                        }

                        // little todo: support left/right docks
                    }
                }

                free(STRUTREPLY);

                Debug::log(LOG, "New dock created, setting default XYWH to: " + std::to_string(PWINDOWINARR->getDefaultPosition().x) + ", " + std::to_string(PWINDOWINARR->getDefaultPosition().y)
                    + ", " + std::to_string(PWINDOWINARR->getDefaultSize().x) + ", " + std::to_string(PWINDOWINARR->getDefaultSize().y));

                PWINDOWINARR->setDock(true);

                // since it's a dock get its monitor from the coords
                const auto CENTERVEC = PWINDOWINARR->getDefaultPosition() + (PWINDOWINARR->getDefaultSize() / 2.f);
                const auto MONITOR = g_pWindowManager->getMonitorFromCoord(CENTERVEC);
                if (MONITOR) {
                    PWINDOWINARR->setMonitor(MONITOR->ID);
                    Debug::log(LOG, "Guessed dock's monitor to be " + std::to_string(MONITOR->ID) + ".");
                } else {
                    Debug::log(LOG, "Couldn't guess dock's monitor. Leaving at " + std::to_string(PWINDOWINARR->getMonitor()) + ".");
                }
            }
        }
    }
    free(wm_type_cookiereply);
    //
    //
    //

    g_pWindowManager->getICCCMSizeHints(PWINDOWINARR);

    if (nextWindowCentered /* Basically means dialog */) {
        auto DELTA = PWINDOWINARR->getPseudoSize() - PWINDOWINARR->getDefaultSize();

        // update
        PWINDOWINARR->setDefaultSize(PWINDOWINARR->getPseudoSize());
        PWINDOWINARR->setDefaultPosition(PWINDOWINARR->getDefaultPosition() - DELTA / 2.f);
    }

    // Check the size and pos rules
    for (auto& rule : ConfigManager::getMatchingRules(windowID)) {
        if (!PWINDOWINARR->getFirstOpen())
            break;

        if (rule.szRule.find("size") == 0) {
            try {
                const auto VALUE = rule.szRule.substr(rule.szRule.find(" ") + 1);
                const auto SIZEX = stoi(VALUE.substr(0, VALUE.find(" ")));
                const auto SIZEY = stoi(VALUE.substr(VALUE.find(" ") + 1));

                Debug::log(LOG, "Rule size, applying to window " + std::to_string(windowID));

                PWINDOWINARR->setDefaultSize(Vector2D(SIZEX, SIZEY));
            } catch (...) {
                Debug::log(LOG, "Rule size failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        } else if (rule.szRule.find("move") == 0) {
            try {
                const auto VALUE = rule.szRule.substr(rule.szRule.find(" ") + 1);
                const auto POSX = stoi(VALUE.substr(0, VALUE.find(" ")));
                const auto POSY = stoi(VALUE.substr(VALUE.find(" ") + 1));

                Debug::log(LOG, "Rule move, applying to window " + std::to_string(windowID));

                PWINDOWINARR->setDefaultPosition(Vector2D(POSX, POSY) + g_pWindowManager->monitors[CURRENTSCREEN].vecPosition);
            } catch (...) {
                Debug::log(LOG, "Rule move failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        }
    }

    //

    PWINDOWINARR->setSize(PWINDOWINARR->getDefaultSize());
    PWINDOWINARR->setPosition(PWINDOWINARR->getDefaultPosition());

    // The anim util will take care of this.
    PWINDOWINARR->setEffectiveSize(PWINDOWINARR->getDefaultSize());
    PWINDOWINARR->setEffectivePosition(PWINDOWINARR->getDefaultPosition());

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(PWINDOWINARR);

    Debug::log(LOG, "Created a new floating window! X: " + std::to_string(PWINDOWINARR->getPosition().x) + ", Y: " + std::to_string(PWINDOWINARR->getPosition().y) + ", W: " + std::to_string(PWINDOWINARR->getSize().x) + ", H:" + std::to_string(PWINDOWINARR->getSize().y) + " ID: " + std::to_string(windowID));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, windowID, XCB_CW_EVENT_MASK, g_pWindowManager->Values);

    // Fix docks
    if (PWINDOWINARR->getDock())
        g_pWindowManager->recalcAllDocks();

    nextWindowCentered = false;

    // Reset flags
    PWINDOWINARR->setConstructed(true);
    PWINDOWINARR->setFirstOpen(false);

    // Fullscreen rule
    if (PWINDOWINARR->getFullscreen()) {
        PWINDOWINARR->setFullscreen(false);
        g_pWindowManager->toggleWindowFullscrenn(PWINDOWINARR->getDrawable());
    }

    return PWINDOWINARR;
}

CWindow* Events::remapWindow(int windowID, bool wasfloating, int forcemonitor) {
    const auto PWINDOWINARR = g_pWindowManager->getWindowFromDrawable(windowID);

    if (!PWINDOWINARR) {
        Debug::log(ERR, "remapWindow called with an invalid window!");
        return nullptr;
    }
        

    PWINDOWINARR->setIsFloating(false);
    PWINDOWINARR->setDirty(true);

    auto PMONITOR = g_pWindowManager->getMonitorFromCursor();
    if (!PMONITOR) {
        Debug::log(ERR, "Monitor was null! (remapWindow) Using 0.");
        PMONITOR = &g_pWindowManager->monitors[0];

        if (g_pWindowManager->monitors.size() == 0) {
            Debug::log(ERR, "Not continuing. Monitors size 0.");
            return nullptr;
        }
    }

    // Check the monitor rule
    for (auto& rule : ConfigManager::getMatchingRules(windowID)) {
        if (!PWINDOWINARR->getFirstOpen())
            break;

        if (rule.szRule.find("monitor") == 0) {
            try {
                const auto MONITOR = stoi(rule.szRule.substr(rule.szRule.find(" ") + 1));

                Debug::log(LOG, "Rule monitor, applying to window " + std::to_string(windowID));

                if (MONITOR > g_pWindowManager->monitors.size() || MONITOR < 0)
                    forcemonitor = -1;
                else
                    forcemonitor = MONITOR;
            } catch (...) {
                Debug::log(LOG, "Rule monitor failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        }

        if (rule.szRule.find("pseudo") == 0) {
            PWINDOWINARR->setIsPseudotiled(true);
        }

        if (rule.szRule.find("fullscreen") == 0) {
            PWINDOWINARR->setFullscreen(true);
        }

        if (rule.szRule.find("workspace") == 0) {
            try {
                const auto WORKSPACE = stoi(rule.szRule.substr(rule.szRule.find(" ") + 1));

                Debug::log(LOG, "Rule workspace, applying to window " + std::to_string(windowID));

                g_pWindowManager->changeWorkspaceByID(WORKSPACE);
                forcemonitor = g_pWindowManager->getWorkspaceByID(WORKSPACE)->getMonitor();
            } catch (...) {
                Debug::log(LOG, "Rule workspace failed, rule: " + rule.szRule + "=" + rule.szValue);
            }
        }
    }

    if (g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow) && forcemonitor == -1 && PMONITOR->ID != g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow)->getMonitor()) {
        // If the monitor of the last window doesnt match the current screen force the monitor of the cursor
        forcemonitor = PMONITOR->ID;
    }

    const auto CURRENTSCREEN = forcemonitor != -1 ? forcemonitor : PMONITOR->ID;
    PWINDOWINARR->setWorkspaceID(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
    PWINDOWINARR->setMonitor(CURRENTSCREEN);

    // Window name
    const auto WINNAME = getWindowName(windowID);
    Debug::log(LOG, "New window got name: " + WINNAME);
    PWINDOWINARR->setName(WINNAME);

    const auto WINCLASSNAME = getClassName(windowID);
    Debug::log(LOG, "New window got class: " + WINCLASSNAME.second);
    PWINDOWINARR->setClassName(WINCLASSNAME.second);

    // For all floating windows, get their default size
    const auto GEOMETRYCOOKIE = xcb_get_geometry(g_pWindowManager->DisplayConnection, windowID);
    const auto GEOMETRY = xcb_get_geometry_reply(g_pWindowManager->DisplayConnection, GEOMETRYCOOKIE, 0);

    if (GEOMETRY) {
        PWINDOWINARR->setDefaultPosition(Vector2D(GEOMETRY->x, GEOMETRY->y));
        PWINDOWINARR->setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));
    } else {
        Debug::log(ERR, "Geometry failed in remap.");

        PWINDOWINARR->setDefaultPosition(Vector2D(0, 0));
        PWINDOWINARR->setDefaultSize(Vector2D(g_pWindowManager->Screen->width_in_pixels / 2.f, g_pWindowManager->Screen->height_in_pixels / 2.f));
    }

    // Check if the workspace has a fullscreen window. if so, remove its' fullscreen status.
    const auto PWORKSPACE = g_pWindowManager->getWorkspaceByID(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
    if (PWORKSPACE && PWORKSPACE->getHasFullscreenWindow()) {
        const auto PFULLSCREENWINDOW = g_pWindowManager->getFullscreenWindowByWorkspace(PWORKSPACE->getID());

        if (PFULLSCREENWINDOW) {
            PFULLSCREENWINDOW->setFullscreen(false);
            PFULLSCREENWINDOW->setDirty(true);
            PWORKSPACE->setHasFullscreenWindow(false);
            g_pWindowManager->setAllWorkspaceWindowsDirtyByID(PWORKSPACE->getID());
        }
    }

    g_pWindowManager->getICCCMSizeHints(PWINDOWINARR);

    // Set the parent
    // check if lastwindow is on our workspace
    if (auto PLASTWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); (PLASTWINDOW && PLASTWINDOW->getWorkspaceID() == g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) || wasfloating || (forcemonitor != -1 && forcemonitor != PMONITOR->ID) || PWINDOWINARR->getWorkspaceID() == SCRATCHPAD_ID) {
        // LastWindow is on our workspace, let's make a new split node

        if (PWINDOWINARR->getWorkspaceID() == SCRATCHPAD_ID)
            PLASTWINDOW = g_pWindowManager->findPreferredOnScratchpad();
        else {
            if (wasfloating || (forcemonitor != -1 && forcemonitor != PMONITOR->ID) || (forcemonitor != -1 && PLASTWINDOW->getWorkspaceID() != g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) || PLASTWINDOW->getIsFloating()) {
                // if it's force monitor, find the first on a workspace.
                if ((forcemonitor != -1 && forcemonitor != PMONITOR->ID) || (forcemonitor != -1 && PLASTWINDOW->getWorkspaceID() != g_pWindowManager->activeWorkspaces[CURRENTSCREEN])) {
                    PLASTWINDOW = g_pWindowManager->findFirstWindowOnWorkspace(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
                } else {
                    // find a window manually by the cursor
                    PLASTWINDOW = g_pWindowManager->findWindowAtCursor();
                }
            }
        }

        if (PLASTWINDOW && PLASTWINDOW->getDrawable() != windowID) {
            CWindow newWindowSplitNode;
            newWindowSplitNode.setPosition(PLASTWINDOW->getPosition());
            newWindowSplitNode.setSize(PLASTWINDOW->getSize());

            newWindowSplitNode.setChildNodeAID(PLASTWINDOW->getDrawable());
            newWindowSplitNode.setChildNodeBID(windowID);

            newWindowSplitNode.setParentNodeID(PLASTWINDOW->getParentNodeID());

            newWindowSplitNode.setWorkspaceID(PWINDOWINARR->getWorkspaceID());
            newWindowSplitNode.setMonitor(PWINDOWINARR->getMonitor());

            // generates a negative node ID
            newWindowSplitNode.generateNodeID();

            // update the parent if exists
            if (const auto PREVPARENT = g_pWindowManager->getWindowFromDrawable(PLASTWINDOW->getParentNodeID()); PREVPARENT) {
                if (PREVPARENT->getChildNodeAID() == PLASTWINDOW->getDrawable()) {
                    PREVPARENT->setChildNodeAID(newWindowSplitNode.getDrawable());
                } else {
                    PREVPARENT->setChildNodeBID(newWindowSplitNode.getDrawable());
                }
            }

            PWINDOWINARR->setParentNodeID(newWindowSplitNode.getDrawable());
            PLASTWINDOW->setParentNodeID(newWindowSplitNode.getDrawable());

            g_pWindowManager->addWindowToVectorSafe(newWindowSplitNode);
        } else {
            PWINDOWINARR->setParentNodeID(0);
        }
    } else {
        // LastWindow is not on our workspace, so set the parent to 0.
        PWINDOWINARR->setParentNodeID(0);
    }

    // For master layout, add the index
    PWINDOWINARR->setMasterChildIndex(g_pWindowManager->getWindowsOnWorkspace(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) - 1);
    // and set master if needed
    if (g_pWindowManager->getWindowsOnWorkspace(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) == 1) // 1 because the current window is already in the arr 
        PWINDOWINARR->setMaster(true);
    

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(PWINDOWINARR);

    // Set real size. No animations in the beginning. Maybe later. TODO?
    PWINDOWINARR->setRealPosition(PWINDOWINARR->getEffectivePosition());
    PWINDOWINARR->setRealSize(PWINDOWINARR->getEffectiveSize());

    Debug::log(LOG, "Created a new tiled window! X: " + std::to_string(PWINDOWINARR->getPosition().x) + ", Y: " + std::to_string(PWINDOWINARR->getPosition().y) + ", W: " + std::to_string(PWINDOWINARR->getSize().x) + ", H:" + std::to_string(PWINDOWINARR->getSize().y) + " ID: " + std::to_string(windowID));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, windowID, XCB_CW_EVENT_MASK, g_pWindowManager->Values);

    // make the last window top (animations look better)
    g_pWindowManager->setAWindowTop(g_pWindowManager->LastWindow);

    // Focus
    g_pWindowManager->setFocusedWindow(windowID);

    // Reset flags
    PWINDOWINARR->setConstructed(true);
    PWINDOWINARR->setFirstOpen(false);

    // Fullscreen rule
    if (PWINDOWINARR->getFullscreen()) {
        PWINDOWINARR->setFullscreen(false);
        g_pWindowManager->toggleWindowFullscrenn(PWINDOWINARR->getDrawable());
    }

    return PWINDOWINARR;
}

void Events::eventMapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_map_request_event_t*>(event);

    // Ignore sequence
    ignoredEvents.push_back(E->sequence);

    // let bar check if it wasnt a tray item
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->ensureTrayClientHidden(E->window, false);

    RETURNIFBAR;

    // Map the window
    xcb_map_window(g_pWindowManager->DisplayConnection, E->window);

    // We check if the window is not on our tile-blacklist and if it is, we have a special treatment procedure for it.
    // this func also sets some stuff

    // Check if it's not unmapped
    CWindow* pNewWindow = nullptr;
    if (g_pWindowManager->isWindowUnmapped(E->window)) {
        Debug::log(LOG, "Window was unmapped, mapping back.");
        g_pWindowManager->moveWindowToMapped(E->window);

        pNewWindow = g_pWindowManager->getWindowFromDrawable(E->window);
    } else {
        if (g_pWindowManager->getWindowFromDrawable(E->window)) {
            Debug::log(LOG, "Window already managed.");
            return;
        }

        if (!g_pWindowManager->shouldBeManaged(E->window)) {
            Debug::log(LOG, "window shouldn't be managed");
            return;
        }

        if (g_pWindowManager->scratchpadActive) {
            KeybindManager::toggleScratchpad("");

            const auto PNEW = g_pWindowManager->findWindowAtCursor();
            g_pWindowManager->LastWindow = PNEW ? PNEW->getDrawable() : 0;
        }

        CWindow window;
        window.setDrawable(E->window);
        g_pWindowManager->addWindowToVectorSafe(window);
        
        if (g_pWindowManager->shouldBeFloatedOnInit(E->window)) {
            Debug::log(LOG, "Window SHOULD be floating on start.");
            pNewWindow = remapFloatingWindow(E->window);
        } else {
            Debug::log(LOG, "Window should NOT be floating on start.");
            pNewWindow = remapWindow(E->window);
        }
    }

    if (!pNewWindow) {
        Debug::log(LOG, "Removing, NULL.");
        g_pWindowManager->removeWindowFromVectorSafe(E->window);
        return;
    }

    // Do post-creation checks.
    g_pWindowManager->doPostCreationChecks(pNewWindow);
    
    // Do ICCCM
    g_pWindowManager->getICCCMWMProtocols(pNewWindow);

    // Do transient checks
    EWMH::checkTransient(E->window);

    // Make all floating windows above
    g_pWindowManager->setAllFloatingWindowsTop();

    // Set not under
    pNewWindow->setUnderFullscreen(false);
    pNewWindow->setDirty(true);

    // EWMH
    EWMH::updateClientList();
    EWMH::setFrameExtents(E->window);
    EWMH::updateWindow(E->window);
}

void Events::eventButtonPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_button_press_event_t*>(event);

    RETURNIFBAR;

    // mouse down!
    g_pWindowManager->mouseKeyDown = E->detail;

    if (const auto PLASTWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); PLASTWINDOW) {

        if (E->detail != 3)
            PLASTWINDOW->setDraggingTiled(!PLASTWINDOW->getIsFloating());

        g_pWindowManager->actingOnWindowFloating = PLASTWINDOW->getDrawable();
        g_pWindowManager->mouseLastPos = g_pWindowManager->getCursorPos();

        if (!PLASTWINDOW->getIsFloating()) {
            const auto PDRAWABLE = PLASTWINDOW->getDrawable();
            if (E->detail != 3) // right click (resize) does not
                KeybindManager::toggleActiveWindowFloating("");

            // refocus
            g_pWindowManager->setFocusedWindow(PDRAWABLE);
        }
    }

    xcb_grab_pointer(g_pWindowManager->DisplayConnection, 0, g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                     g_pWindowManager->Screen->root, XCB_NONE, XCB_CURRENT_TIME);
}

void Events::eventButtonRelease(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_button_release_event_t*>(event);

    RETURNIFBAR;

    const auto PACTINGWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->actingOnWindowFloating);

    // ungrab the mouse ptr
    xcb_ungrab_pointer(g_pWindowManager->DisplayConnection, XCB_CURRENT_TIME);

    if (PACTINGWINDOW) {
        PACTINGWINDOW->setDirty(true);

        if (PACTINGWINDOW->getDraggingTiled()) {
            g_pWindowManager->LastWindow = PACTINGWINDOW->getDrawable();
            KeybindManager::toggleActiveWindowFloating("");
        }
            
    }

    g_pWindowManager->actingOnWindowFloating = 0;
    g_pWindowManager->mouseKeyDown = 0;
}

void Events::eventKeyPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_key_press_event_t*>(event);

    RETURNIFBAR;

    const auto KEYSYM = KeybindManager::getKeysymFromKeycode(E->detail);
    const auto IGNOREDMOD = KeybindManager::modToMask(ConfigManager::getString("ignore_mod"));

    for (auto& keybind : KeybindManager::keybinds) {
        if (keybind.getKeysym() != 0 && keybind.getKeysym() == KEYSYM && ((keybind.getMod() == E->state) || ((keybind.getMod() | IGNOREDMOD) == E->state))) {
            keybind.getDispatcher()(keybind.getCommand());
            return;
            // TODO: fix duplicating keybinds
        }
    }
}

void Events::eventMotionNotify(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_motion_notify_event_t*>(event);

    RETURNIFBAR;

    if (!g_pWindowManager->mouseKeyDown)
        return; // mouse up.

    if (!g_pWindowManager->actingOnWindowFloating)
        return; // not acting, return.

    // means we are holding super
    const auto POINTERPOS = g_pWindowManager->getCursorPos();
    const auto POINTERDELTA = Vector2D(POINTERPOS) - g_pWindowManager->mouseLastPos;

    const auto PACTINGWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->actingOnWindowFloating);

    if (!PACTINGWINDOW) {
        Debug::log(ERR, "ActingWindow not null but doesn't exist?? (Died?)");
        g_pWindowManager->actingOnWindowFloating = 0;
        return;
    }

    if (abs(POINTERDELTA.x) < 1 && abs(POINTERDELTA.y) < 1)
        return; // micromovements

    if (g_pWindowManager->mouseKeyDown == 1) {
        // moving
        PACTINGWINDOW->setPosition(PACTINGWINDOW->getPosition() + POINTERDELTA);
        PACTINGWINDOW->setEffectivePosition(PACTINGWINDOW->getPosition());
        PACTINGWINDOW->setDefaultPosition(PACTINGWINDOW->getPosition());
        PACTINGWINDOW->setRealPosition(PACTINGWINDOW->getPosition());

        // update workspace if needed
        if (g_pWindowManager->getMonitorFromCursor()) {
            const auto WORKSPACE = g_pWindowManager->activeWorkspaces[g_pWindowManager->getMonitorFromCursor()->ID];
            PACTINGWINDOW->setWorkspaceID(WORKSPACE);
        } else {
            Debug::log(WARN, "Monitor was nullptr! Ignoring workspace change in MouseMoveEvent.");
        } 

        PACTINGWINDOW->setDirty(true);
    } else if (g_pWindowManager->mouseKeyDown == 3) {

        if (!PACTINGWINDOW->getIsFloating()) {
            g_pWindowManager->processCursorDeltaOnWindowResizeTiled(PACTINGWINDOW, POINTERDELTA);
        } else {
            // resizing
            PACTINGWINDOW->setSize(PACTINGWINDOW->getSize() + POINTERDELTA);
            // clamp
            PACTINGWINDOW->setSize(Vector2D(std::clamp(PACTINGWINDOW->getSize().x, (double)30, (double)999999), std::clamp(PACTINGWINDOW->getSize().y, (double)30, (double)999999)));

            // apply to other
            PACTINGWINDOW->setDefaultSize(PACTINGWINDOW->getSize());
            PACTINGWINDOW->setEffectiveSize(PACTINGWINDOW->getSize());
            PACTINGWINDOW->setRealSize(PACTINGWINDOW->getSize());
            PACTINGWINDOW->setPseudoSize(PACTINGWINDOW->getSize());
        }

        PACTINGWINDOW->setDirty(true);
    }

    g_pWindowManager->mouseLastPos = POINTERPOS;
}

void Events::eventExpose(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_expose_event_t*>(event);

    // nothing
}

void Events::eventClientMessage(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_client_message_event_t*>(event);

    if (!g_pWindowManager->statusBar)
        g_pWindowManager->handleClientMessage(E); // Client message handling

    RETURNIFMAIN; // Only for the bar

    // Tray clients

    if (E->type == HYPRATOMS["_NET_SYSTEM_TRAY_OPCODE"] && E->format == 32) {
        // Tray request!

        Debug::log(LOG, "Docking a window to the tray!");

        if (E->data.data32[1] == 0) { // Request dock
            const xcb_window_t CLIENT = E->data.data32[2];

            uint32_t values[3] = {0,0,0};

            values[0] =  XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_RESIZE_REDIRECT;

            xcb_change_window_attributes(g_pWindowManager->DisplayConnection, CLIENT,
                                        XCB_CW_EVENT_MASK, values);

            // get XEMBED

            const auto XEMBEDCOOKIE = xcb_get_property(g_pWindowManager->DisplayConnection, 0, CLIENT, HYPRATOMS["_XEMBED_INFO"],
                                                        XCB_GET_PROPERTY_TYPE_ANY, 0, 64);

            xcb_generic_error_t* err;
            const auto XEMBEDREPLY  = xcb_get_property_reply(g_pWindowManager->DisplayConnection, XEMBEDCOOKIE, &err);

            if (!XEMBEDREPLY || err || XEMBEDREPLY->length == 0) {
                Debug::log(ERR, "Tray dock opcode recieved with no XEmbed?");
                if (err)
                    Debug::log(ERR, "Error code: " + std::to_string(err->error_code));
                free(XEMBEDREPLY);
                return;
            }

            const uint32_t* XEMBEDPROP = (uint32_t*)xcb_get_property_value(XEMBEDREPLY);
            Debug::log(LOG, "XEmbed recieved with format " + std::to_string(XEMBEDREPLY->format) + ", length " + std::to_string(XEMBEDREPLY->length)
                                    + ", version " + std::to_string(XEMBEDPROP[0]) + ", flags " + std::to_string(XEMBEDPROP[1]));

            const auto XEMBEDVERSION = XEMBEDPROP[0] > 1 ? 1 : XEMBEDPROP[0];

            free(XEMBEDREPLY);

            xcb_reparent_window(g_pWindowManager->DisplayConnection, CLIENT, g_pWindowManager->statusBar->getWindowID(), 0, 0);
        
            // icon sizes are barY - 2 - pad: 1
            values[0] = ConfigManager::getInt("bar:height") - 2 < 1 ? 1 : ConfigManager::getInt("bar:height") - 2;
            values[1] = values[0];

            xcb_configure_window(g_pWindowManager->DisplayConnection, CLIENT, XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_WIDTH, values);


            // Notify the thing we did it
            uint8_t buf[32] = {NULL};
            xcb_client_message_event_t* event = (xcb_client_message_event_t*)buf;
            event->response_type = XCB_CLIENT_MESSAGE;
            event->window = CLIENT;
            event->type = HYPRATOMS["_XEMBED"];
            event->format = 32;
            event->data.data32[0] = XCB_CURRENT_TIME;
            event->data.data32[1] = 0;
            event->data.data32[2] = g_pWindowManager->statusBar->getWindowID();
            event->data.data32[3] = XEMBEDVERSION;
            xcb_send_event(g_pWindowManager->DisplayConnection, 0, CLIENT, XCB_EVENT_MASK_NO_EVENT, (char*)event);

            // put it into the save set
            xcb_change_save_set(g_pWindowManager->DisplayConnection, XCB_SET_MODE_INSERT, CLIENT);

            // make a tray client
            CTrayClient newTrayClient;
            newTrayClient.window = CLIENT;
            newTrayClient.XEVer = XEMBEDVERSION;

            g_pWindowManager->trayclients.push_back(newTrayClient);

            xcb_map_window(g_pWindowManager->DisplayConnection, CLIENT);
        }
    }
}

void Events::eventConfigure(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_configure_request_event_t*>(event);

    Debug::log(LOG, "Window " + std::to_string(E->window) + " requests XY: " + std::to_string(E->x) + ", " + std::to_string(E->y) + ", WH: " + std::to_string(E->width) + "x" + std::to_string(E->height));

    auto *const PWINDOW = g_pWindowManager->getWindowFromDrawable(E->window);

    if (!PWINDOW) {
        Debug::log(LOG, "CONFIGURE: Window doesn't exist, ignoring.");
        return;
    }

    if (!PWINDOW->getIsFloating()) {
        Debug::log(LOG, "CONFIGURE: Window isn't floating, ignoring.");
        return;
    }

    PWINDOW->setDefaultPosition(Vector2D(E->x, E->y));
    PWINDOW->setDefaultSize(Vector2D(E->width, E->height));
    PWINDOW->setEffectiveSize(PWINDOW->getDefaultSize());
    PWINDOW->setEffectivePosition(PWINDOW->getDefaultPosition());
}

void Events::eventRandRScreenChange(xcb_generic_event_t* event) {

    // fix sus randr events, that sometimes happen
    // it will spam these for no reason
    // so we check if we have > 9 consecutive randr events less than 1s between each
    // and if so, we stop listening for them
    const auto DELTA = std::chrono::duration_cast<std::chrono::milliseconds>(lastRandREvent - std::chrono::high_resolution_clock::now());

    if (susRandREventNo < 10) {
        if (DELTA.count() <= 1000) {
            susRandREventNo += 1;
            Debug::log(WARN, "Suspicious RandR event no. " + std::to_string(susRandREventNo) + "!");
            if (susRandREventNo > 9)
                Debug::log(WARN, "Disabling RandR event listening because of excess suspicious RandR events (bug!)");
        }
        else
            susRandREventNo = 0;
    }

    if (susRandREventNo > 9)
        return; 
    // randr sus fixed
    //

    // redetect screens
    g_pWindowManager->monitors.clear();
    g_pWindowManager->setupRandrMonitors();

    // Detect monitors that are incorrect
    // Orphaned workspaces
    for (auto& w : g_pWindowManager->workspaces) {
        if (w.getMonitor() >= g_pWindowManager->monitors.size())
            w.setMonitor(0);
    }

    // Empty monitors
    bool fineMonitors[g_pWindowManager->monitors.size()];
    for (int i = 0; i < g_pWindowManager->monitors.size(); ++i)
        fineMonitors[i] = false;

    for (auto& w : g_pWindowManager->workspaces) {
        fineMonitors[w.getMonitor()] = true;
    }

    for (int i = 0; i < g_pWindowManager->monitors.size(); ++i) {
        if (!fineMonitors[i]) {
            // add a workspace
            CWorkspace newWorkspace;
            newWorkspace.setMonitor(i);
            newWorkspace.setID(g_pWindowManager->getHighestWorkspaceID() + 1);
            newWorkspace.setHasFullscreenWindow(false);
            newWorkspace.setLastWindow(0);
            g_pWindowManager->workspaces.push_back(newWorkspace);
        }
    }

    // reload the config to update the bar too
    ConfigManager::loadConfigLoadVars();

    // Make all windows dirty and recalc all workspaces
    g_pWindowManager->recalcAllWorkspaces();
}