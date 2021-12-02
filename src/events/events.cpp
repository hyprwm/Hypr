#include "events.hpp"

gpointer handle(gpointer data) {
    int lazyUpdateCounter = 0;

    while (1) {
        // wait for the main thread to be idle
        while (g_pWindowManager->mainThreadBusy) {
            ;
        }

        // set state to let the main thread know to wait.
        g_pWindowManager->animationUtilBusy = true;

        // update animations.
        AnimationUtil::move();
        //

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

    g_pWindowManager->barThread = g_thread_new("Bar", handle, nullptr);

    if (!g_pWindowManager->barThread) {
        Debug::log(ERR, "Gthread failed!");
        return;
    }

    /*g_pWindowManager->barThread = new std::thread([&]() {
        for (;;) {
            handle();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / ConfigManager::getInt("max_fps")));
        }
    });*/
}

void Events::eventEnter(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_enter_notify_event_t*>(event);

    // Just focus it and update.
    g_pWindowManager->setFocusedWindow(E->event);

    if(const auto PENTERWINDOW = g_pWindowManager->getWindowFromDrawable(E->event)) {
        PENTERWINDOW->setDirty(true);
    }
}

void Events::eventLeave(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_leave_notify_event_t*>(event);

    //
}

void Events::eventDestroy(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_destroy_notify_event_t*>(event);

    g_pWindowManager->closeWindowAllChecks(E->window);

    // refocus on new window
    g_pWindowManager->refocusWindowOnClosed();
}

void Events::eventUnmapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_unmap_notify_event_t*>(event);

    g_pWindowManager->closeWindowAllChecks(E->window);

    // refocus on new window
    g_pWindowManager->refocusWindowOnClosed();
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

    if (window.getDefaultSize().x < 20 || window.getDefaultSize().y < 20) {
        // min size
        window.setDefaultSize(Vector2D( std::clamp(window.getDefaultSize().x, (double)20, (double)99999),
                                        std::clamp(window.getDefaultSize().y, (double)20, (double)99999)));
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
        Debug::log(LOG, "No preferred type found.");
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
            }
        }
    }
    free(wm_type_cookiereply);
    //
    //
    //

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(&window);

    // Set real size. No animations in the beginning. Maybe later. TODO?
    window.setRealPosition(window.getEffectivePosition());
    window.setRealSize(window.getEffectiveSize());

    // Add to arr
    g_pWindowManager->addWindowToVectorSafe(window);

    Debug::log(LOG, "Created a new floating window! X: " + std::to_string(window.getPosition().x) + ", Y: " + std::to_string(window.getPosition().y) + ", W: " + std::to_string(window.getSize().x) + ", H:" + std::to_string(window.getSize().y) + " ID: " + std::to_string(windowID));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, windowID, XCB_CW_EVENT_MASK, g_pWindowManager->Values);

    g_pWindowManager->setFocusedWindow(windowID);

    // Make all floating windows above
    g_pWindowManager->setAllFloatingWindowsTop();

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

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(&window);

    // Set real size. No animations in the beginning. Maybe later. TODO?
    window.setRealPosition(window.getEffectivePosition());
    window.setRealSize(window.getEffectiveSize());

    // Add to arr
    g_pWindowManager->addWindowToVectorSafe(window);

    Debug::log(LOG, "Created a new tiled window! X: " + std::to_string(window.getPosition().x) + ", Y: " + std::to_string(window.getPosition().y) + ", W: " + std::to_string(window.getSize().x) + ", H:" + std::to_string(window.getSize().y) + " ID: " + std::to_string(windowID));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, windowID, XCB_CW_EVENT_MASK, g_pWindowManager->Values);

    g_pWindowManager->setFocusedWindow(windowID);

    // Make all floating windows above
    g_pWindowManager->setAllFloatingWindowsTop();

    return g_pWindowManager->getWindowFromDrawable(windowID);
}

void Events::eventMapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_map_request_event_t*>(event);

    // Map the window
    xcb_map_window(g_pWindowManager->DisplayConnection, E->window);

    // make sure it's not the bar!
    if (E->window == g_pWindowManager->barWindowID)
        return;

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
}

void Events::eventButtonPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_button_press_event_t*>(event);

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

    const auto KEYSYM = KeybindManager::getKeysymFromKeycode(E->detail);

    for (auto& keybind : KeybindManager::keybinds) {
        if (keybind.getKeysym() != 0 && keybind.getKeysym() == KEYSYM && KeybindManager::modToMask(keybind.getMod()) == E->state) {
            keybind.getDispatcher()(keybind.getCommand());
            return;
            // TODO: fix duplicating keybinds
        }
    }
}

void Events::eventMotionNotify(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_motion_notify_event_t*>(event);

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