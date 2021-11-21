#include "events.hpp"

void Events::redraw() {
    xcb_expose_event_t exposeEvent;
    exposeEvent.window = g_pWindowManager->statusBar.getWindowID();
    exposeEvent.response_type = XCB_EXPOSE;
    exposeEvent.x = 0;
    exposeEvent.y = 0;
    exposeEvent.width = g_pWindowManager->Screen->width_in_pixels;
    exposeEvent.height = g_pWindowManager->Screen->height_in_pixels;
    xcb_send_event(g_pWindowManager->DisplayConnection, false, g_pWindowManager->statusBar.getWindowID(), XCB_EVENT_MASK_EXPOSURE, (char*)&exposeEvent);
    xcb_flush(g_pWindowManager->DisplayConnection);
}

void handle(sigval val) {
    //Events::redraw();
    g_pWindowManager->statusBar.draw();
}

void Events::setThread() {
    sigevent eventsig;
    eventsig.sigev_notify = SIGEV_THREAD;
    eventsig.sigev_notify_function = handle;
    eventsig.sigev_notify_attributes = NULL;
    eventsig.sigev_value.sival_ptr = &timerid;
    timer_create(CLOCK_REALTIME, &eventsig, &timerid);

    itimerspec t = {{0, 1000 * (1000 / MAX_FPS)}, {1, 0}};
    timer_settime(timerid, 0, &t, 0);
}

void Events::eventEnter(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_enter_notify_event_t*>(event);

    // Just focus it and update.
    g_pWindowManager->setFocusedWindow(E->event);

    //                                           vvv insallah no segfaults
    g_pWindowManager->getWindowFromDrawable(E->event)->setDirty(true);
}

void Events::eventLeave(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_leave_notify_event_t*>(event);

    //
}

void Events::eventDestroy(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_destroy_notify_event_t*>(event);
    xcb_kill_client(g_pWindowManager->DisplayConnection, E->window);

    // fix last window
    const auto CLOSEDWINDOW = g_pWindowManager->getWindowFromDrawable(E->window);
    if (CLOSEDWINDOW) {
        g_pWindowManager->fixWindowOnClose(CLOSEDWINDOW);

        // delete off of the arr
        g_pWindowManager->removeWindowFromVectorSafe(E->window);
    }
}

void Events::eventMapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_map_request_event_t*>(event);

    // make sure it's not the bar!
    if (E->window == g_pWindowManager->statusBar.getWindowID())
        return;

    // Map the window
    xcb_map_window(g_pWindowManager->DisplayConnection, E->window);

    // Do the setup of the window's params and stuf
    CWindow window;
    window.setDrawable(E->window);
    window.setIsFloating(false);
    window.setDirty(true);
    window.setWorkspaceID(g_pWindowManager->activeWorkspace->getID());

    // Also sets the old one
    g_pWindowManager->calculateNewWindowParams(&window);

    // Focus
    g_pWindowManager->setFocusedWindow(E->window);

    // Add to arr
    g_pWindowManager->addWindowToVectorSafe(window);

    Debug::log(LOG, "Created a new window! X: " + std::to_string(window.getPosition().x) + ", Y: " + std::to_string(window.getPosition().y) + ", W: "
        + std::to_string(window.getSize().x) + ", H:" + std::to_string(window.getSize().y) + " ID: " + std::to_string(E->window));

    // Set map values
    g_pWindowManager->Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_pWindowManager->DisplayConnection, E->window, XCB_CW_EVENT_MASK, g_pWindowManager->Values);
 
    g_pWindowManager->setFocusedWindow(E->window);
}

void Events::eventKeyPress(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_key_press_event_t*>(event);

    const auto KEYSYM = KeybindManager::getKeysymFromKeycode(E->detail);

    for (auto& keybind : KeybindManager::keybinds) {
        if (keybind.getKeysym() != 0 && keybind.getKeysym() == KEYSYM && KeybindManager::modToMask(keybind.getMod()) == E->state) {
            keybind.getDispatcher()(keybind.getCommand());
        }
    }
}

void Events::eventExpose(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_expose_event_t*>(event);

    // Draw the bar
    g_pWindowManager->statusBar.draw();
}