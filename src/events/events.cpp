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


    const auto PENTERWINDOW = g_pWindowManager->getWindowFromDrawable(E->event);

    if (!PENTERWINDOW)
        return; // wut

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

    if (!PCLOSEDWINDOW)
        return; // bullshit window?

    if (PCLOSEDWINDOW->getIsFloating())
        g_pWindowManager->moveWindowToUnmapped(E->event);  // If it's floating, just unmap it.
    else
        g_pWindowManager->closeWindowAllChecks(E->window);

    // refocus on new window
    g_pWindowManager->refocusWindowOnClosed();

    // EWMH
    EWMH::updateClientList();
}

CWindow* Events::remapFloatingWindow(int windowID, int forcemonitor) {
    CWindow window;
    window.setDrawable(windowID);
    window.setIsFloating(true);
    window.setDirty(true);
    if (!g_pWindowManager->getMonitorFromCursor()) {
        Debug::log(ERR, "Monitor was null! (remapWindow)");
        // rip! we cannot continue.
    }
    const auto CURRENTSCREEN = forcemonitor != -1 ? forcemonitor : g_pWindowManager->getMonitorFromCursor()->ID;
    window.setWorkspaceID(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
    window.setMonitor(CURRENTSCREEN);

    // Window name
    const auto WINNAME = getWindowName(windowID);
    Debug::log(LOG, "New window got name: " + WINNAME);
    window.setName(WINNAME);

    const auto WINCLASSNAME = getClassName(windowID);
    Debug::log(LOG, "New window got class: " + WINCLASSNAME.second);
    window.setClassName(WINCLASSNAME.second);

    // For all floating windows, get their default size
    const auto GEOMETRYCOOKIE   = xcb_get_geometry(g_pWindowManager->DisplayConnection, windowID);
    const auto GEOMETRY         = xcb_get_geometry_reply(g_pWindowManager->DisplayConnection, GEOMETRYCOOKIE, 0);

    if (GEOMETRY) {
        window.setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition);
        window.setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));
    } else {
        Debug::log(ERR, "Geometry failed in remap.");

        window.setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition);
        window.setDefaultSize(Vector2D(g_pWindowManager->Screen->width_in_pixels / 2.f, g_pWindowManager->Screen->height_in_pixels / 2.f));
    }

    if (window.getDefaultSize().x < 40 || window.getDefaultSize().y < 40) {
        // min size
        window.setDefaultSize(Vector2D(std::clamp(window.getDefaultSize().x, (double)40, (double)99999),
                                       std::clamp(window.getDefaultSize().y, (double)40, (double)99999)));
    }

    if (nextWindowCentered) {
        nextWindowCentered = false;

        window.setDefaultPosition(g_pWindowManager->monitors[CURRENTSCREEN].vecPosition + g_pWindowManager->monitors[CURRENTSCREEN].vecSize / 2.f - window.getDefaultSize() / 2.f);
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
                window.setImmovable(true);
                window.setNoInterventions(true);

                window.setDefaultPosition(Vector2D(GEOMETRY->x, GEOMETRY->y));
                window.setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));

                Debug::log(LOG, "New dock created, setting default XYWH to: " + std::to_string(GEOMETRY->x) + ", " + std::to_string(GEOMETRY->y)
                    + ", " + std::to_string(GEOMETRY->width) + ", " + std::to_string(GEOMETRY->height));

                window.setDock(true);
            }
        }
    }
    free(wm_type_cookiereply);
    //
    //
    //

    window.setSize(window.getDefaultSize());
    window.setPosition(window.getDefaultPosition());

    // The anim util will take care of this.
    window.setEffectiveSize(window.getDefaultSize());
    window.setEffectivePosition(window.getDefaultPosition());

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(&window);

    // Add to arr
    g_pWindowManager->addWindowToVectorSafe(window);

    Debug::log(LOG, "Created a new floating window! X: " + std::to_string(window.getPosition().x) + ", Y: " + std::to_string(window.getPosition().y) + ", W: " + std::to_string(window.getSize().x) + ", H:" + std::to_string(window.getSize().y) + " ID: " + std::to_string(windowID));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, windowID, XCB_CW_EVENT_MASK, g_pWindowManager->Values);

    // Make all floating windows above
    g_pWindowManager->setAllFloatingWindowsTop();

    // Fix docks
    if (window.getDock())
        g_pWindowManager->recalcAllDocks();

    return g_pWindowManager->getWindowFromDrawable(windowID);
}

CWindow* Events::remapWindow(int windowID, bool wasfloating, int forcemonitor) {
    // Do the setup of the window's params and stuf
    CWindow window;
    window.setDrawable(windowID);
    window.setIsFloating(false);
    window.setDirty(true);
    if (!g_pWindowManager->getMonitorFromCursor()) {
        Debug::log(ERR, "Monitor was null! (remapWindow)");
        // rip! we cannot continue.
    }
    const auto CURRENTSCREEN = forcemonitor != -1 ? forcemonitor : g_pWindowManager->getMonitorFromCursor()->ID;
    window.setWorkspaceID(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]);
    window.setMonitor(CURRENTSCREEN);

    // Window name
    const auto WINNAME = getWindowName(windowID);
    Debug::log(LOG, "New window got name: " + WINNAME);
    window.setName(WINNAME);

    const auto WINCLASSNAME = getClassName(windowID);
    Debug::log(LOG, "New window got class: " + WINCLASSNAME.second);
    window.setClassName(WINCLASSNAME.second);

    // For all floating windows, get their default size
    const auto GEOMETRYCOOKIE = xcb_get_geometry(g_pWindowManager->DisplayConnection, windowID);
    const auto GEOMETRY = xcb_get_geometry_reply(g_pWindowManager->DisplayConnection, GEOMETRYCOOKIE, 0);

    if (GEOMETRY) {
        window.setDefaultPosition(Vector2D(GEOMETRY->x, GEOMETRY->y));
        window.setDefaultSize(Vector2D(GEOMETRY->width, GEOMETRY->height));
    } else {
        Debug::log(ERR, "Geometry failed in remap.");

        window.setDefaultPosition(Vector2D(0, 0));
        window.setDefaultSize(Vector2D(g_pWindowManager->Screen->width_in_pixels / 2.f, g_pWindowManager->Screen->height_in_pixels / 2.f));
    }

    // Set the parent
    // check if lastwindow is on our workspace
    if (auto PLASTWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); (PLASTWINDOW && PLASTWINDOW->getWorkspaceID() == g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) || wasfloating) {
        // LastWindow is on our workspace, let's make a new split node

        if (wasfloating || PLASTWINDOW->getIsFloating()) {
            // find a window manually
            PLASTWINDOW = g_pWindowManager->findWindowAtCursor();
        }

        if (PLASTWINDOW) {
            CWindow newWindowSplitNode;
            newWindowSplitNode.setPosition(PLASTWINDOW->getPosition());
            newWindowSplitNode.setSize(PLASTWINDOW->getSize());

            newWindowSplitNode.setChildNodeAID(PLASTWINDOW->getDrawable());
            newWindowSplitNode.setChildNodeBID(windowID);

            newWindowSplitNode.setParentNodeID(PLASTWINDOW->getParentNodeID());

            newWindowSplitNode.setWorkspaceID(window.getWorkspaceID());
            newWindowSplitNode.setMonitor(window.getMonitor());

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

            window.setParentNodeID(newWindowSplitNode.getDrawable());
            PLASTWINDOW->setParentNodeID(newWindowSplitNode.getDrawable());

            g_pWindowManager->addWindowToVectorSafe(newWindowSplitNode);
        } else {
            window.setParentNodeID(0);
        }
    } else {
        // LastWindow is not on our workspace, so set the parent to 0.
        window.setParentNodeID(0);
    }

    // For master layout, add the index
    window.setMasterChildIndex(g_pWindowManager->getWindowsOnWorkspace(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) - 1);
    // and set master if needed
    if (g_pWindowManager->getWindowsOnWorkspace(g_pWindowManager->activeWorkspaces[CURRENTSCREEN]) == 0) 
        window.setMaster(true);


    // Add to arr
    g_pWindowManager->addWindowToVectorSafe(window);

    // Now we need to modify the copy in the array.
    const auto PWINDOWINARR = g_pWindowManager->getWindowFromDrawable(windowID);


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

    // Make all floating windows above
    g_pWindowManager->setAllFloatingWindowsTop();

    return PWINDOWINARR;
}

void Events::eventMapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_map_request_event_t*>(event);

    // let bar check if it wasnt a tray item
    if (g_pWindowManager->statusBar)
        g_pWindowManager->statusBar->ensureTrayClientHidden(E->window, false);

    RETURNIFBAR;

    // Map the window
    xcb_map_window(g_pWindowManager->DisplayConnection, E->window);

    // make sure it's not the bar!
    if (E->window == g_pWindowManager->barWindowID)
        return;

    // Check if it's not unmapped
    if (g_pWindowManager->isWindowUnmapped(E->window)) {
        g_pWindowManager->moveWindowToMapped(E->window);
        return;
    }

    // We check if the window is not on our tile-blacklist and if it is, we have a special treatment procedure for it.
    // this func also sets some stuff

    CWindow* pNewWindow = nullptr;
    if (g_pWindowManager->shouldBeFloatedOnInit(E->window)) {
        Debug::log(LOG, "Window SHOULD be floating on start.");
        pNewWindow = remapFloatingWindow(E->window);
    } else {
        Debug::log(LOG, "Window should NOT be floating on start.");
        pNewWindow = remapWindow(E->window);
    }

    if (!pNewWindow)
        return;

    // Do post-creation checks.
    g_pWindowManager->doPostCreationChecks(pNewWindow);
    
    // Do ICCCM
    g_pWindowManager->getICCCMWMProtocols(pNewWindow);

    // Set not under
    pNewWindow->setUnderFullscreen(false);
    pNewWindow->setDirty(true);

    // EWMH
    EWMH::updateClientList();
}

void Events::eventButtonPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_button_press_event_t*>(event);

    RETURNIFBAR;

    // mouse down!
    g_pWindowManager->mouseKeyDown = E->detail;
    xcb_grab_pointer(g_pWindowManager->DisplayConnection, 0, g_pWindowManager->Screen->root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                     g_pWindowManager->Screen->root, XCB_NONE, XCB_CURRENT_TIME);

    if (const auto PLASTWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); PLASTWINDOW) {
        if (PLASTWINDOW->getIsFloating()) {
            g_pWindowManager->actingOnWindowFloating = PLASTWINDOW->getDrawable();
            g_pWindowManager->mouseLastPos = g_pWindowManager->getCursorPos();
        }
    }
}

void Events::eventButtonRelease(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_button_release_event_t*>(event);

    RETURNIFBAR;

    // ungrab the mouse ptr
    xcb_ungrab_pointer(g_pWindowManager->DisplayConnection, XCB_CURRENT_TIME);
    const auto PACTINGWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->actingOnWindowFloating);
    if (PACTINGWINDOW)
        PACTINGWINDOW->setDirty(true);
    g_pWindowManager->actingOnWindowFloating = 0;
    g_pWindowManager->mouseKeyDown = 0;
}

void Events::eventKeyPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_key_press_event_t*>(event);

    RETURNIFBAR;

    const auto KEYSYM = KeybindManager::getKeysymFromKeycode(E->detail);

    for (auto& keybind : KeybindManager::keybinds) {
        if (keybind.getKeysym() != 0 && keybind.getKeysym() == KEYSYM && keybind.getMod() == E->state) {
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
            PACTINGWINDOW->setWorkspaceID(-1);
        } 

        PACTINGWINDOW->setDirty(true);
    } else if (g_pWindowManager->mouseKeyDown == 3) {
        // resizing
        PACTINGWINDOW->setSize(PACTINGWINDOW->getSize() + POINTERDELTA);
        // clamp
        PACTINGWINDOW->setSize(Vector2D(std::clamp(PACTINGWINDOW->getSize().x, (double)30, (double)999999), std::clamp(PACTINGWINDOW->getSize().y, (double)30, (double)999999)));

        // apply to other
        PACTINGWINDOW->setDefaultSize(PACTINGWINDOW->getSize());
        PACTINGWINDOW->setEffectiveSize(PACTINGWINDOW->getSize());
        PACTINGWINDOW->setRealSize(PACTINGWINDOW->getSize());

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