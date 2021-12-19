#include "ewmh.hpp"
#include "../windowManager.hpp"

void EWMH::setupInitEWMH() {
    Debug::log(LOG, "EWMH init!");

    EWMHwindow = xcb_generate_id(g_pWindowManager->DisplayConnection);

    Debug::log(LOG, "Allocated ID " + std::to_string(EWMHwindow) + " for the EWMH window.");

    uint32_t values[1] = {1};
    xcb_create_window(g_pWindowManager->DisplayConnection, XCB_COPY_FROM_PARENT, EWMHwindow, g_pWindowManager->Screen->root,
        -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, values);

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, EWMHwindow, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW, 32, 1, &EWMHwindow);
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, EWMHwindow, HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["UTF8_STRING"], 8, strlen("Hypr"), "Hypr");
    
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["UTF8_STRING"], 8, strlen("Hypr"), "Hypr");
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW, 32, 1, &EWMHwindow);
    
    // Atoms EWMH

    xcb_atom_t supportedAtoms[HYPRATOMS.size()];
    int i = 0;
    for (auto& a : HYPRATOMS) {
        supportedAtoms[i] = a.second;
        i++;
    }

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_SUPPORTED"], XCB_ATOM_ATOM, 32, sizeof(supportedAtoms) / sizeof(xcb_atom_t), supportedAtoms);

    // delete workarea
    xcb_delete_property(g_pWindowManager->DisplayConnection, g_pWindowManager->Screen->root, HYPRATOMS["_NET_WORKAREA"]);

    Debug::log(LOG, "EWMH init done.");
}

void EWMH::updateCurrentWindow(xcb_window_t w) {
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_ACTIVE_WINDOW"], XCB_ATOM_WINDOW, 32, 1, &w);
}

void EWMH::updateClientList() {
    std::vector<xcb_window_t> windowsList;
    for (auto& w : g_pWindowManager->windows)
        if (w.getDrawable() > 0)
            windowsList.push_back(w.getDrawable());
    for (auto& w : g_pWindowManager->unmappedWindows)
        windowsList.push_back(w.getDrawable());

    // hack
    xcb_window_t* ArrWindowList = &windowsList[0];

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_CLIENT_LIST"], XCB_ATOM_WINDOW,
        32, windowsList.size(), ArrWindowList);
}

void EWMH::setFrameExtents(xcb_window_t w) {
	uint32_t extents[4] = {ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size"), ConfigManager::getInt("border_size")};
	xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, w, HYPRATOMS["_NET_FRAME_EXTENTS"], XCB_ATOM_CARDINAL, 32, 4, &extents);
}
