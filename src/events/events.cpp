#include "events.hpp"

void Events::eventEnter(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_enter_notify_event_t*>(event);

    // Just focus it and update.
    WindowManager::setFocusedWindow(E->event);

    //                                           vvv insallah no segfaults
    WindowManager::getWindowFromDrawable(E->event)->setDirty(true);
}

void Events::eventLeave(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_leave_notify_event_t*>(event);

    //
}

void Events::eventDestroy(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_destroy_notify_event_t*>(event);
    xcb_kill_client(WindowManager::DisplayConnection, E->window);

    // fix last window
    const auto CLOSEDWINDOW = WindowManager::getWindowFromDrawable(E->window);
    if (CLOSEDWINDOW) {
        WindowManager::fixWindowOnClose(CLOSEDWINDOW);

        // delete off of the arr
        WindowManager::removeWindowFromVectorSafe(E->window);
    }
}

void Events::eventMapWindow(xcb_generic_event_t* event) {
    const auto E = reinterpret_cast<xcb_map_request_event_t*>(event);

    // Map the window
    xcb_map_window(WindowManager::DisplayConnection, E->window);

    // Do the setup of the window's params and stuf
    CWindow window;
    window.setDrawable(E->window);
    window.setIsFloating(false);
    window.setDirty(true);

    // Also sets the old one
    WindowManager::calculateNewWindowParams(&window);

    // Focus
    WindowManager::setFocusedWindow(E->window);

    // Add to arr
    WindowManager::addWindowToVectorSafe(window);

    Debug::log(LOG, "Created a new window! X: " + std::to_string(window.getPosition().x) + ", Y: " + std::to_string(window.getPosition().y) + ", W: "
        + std::to_string(window.getSize().x) + ", H:" + std::to_string(window.getSize().y) + " ID: " + std::to_string(E->window));

    // Set map values
    WindowManager::Values[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(WindowManager::DisplayConnection, E->window, XCB_CW_EVENT_MASK, WindowManager::Values);
 
    WindowManager::setFocusedWindow(E->window);
}