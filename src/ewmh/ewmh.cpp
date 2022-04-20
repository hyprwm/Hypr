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
        if (w.getDrawable() > 0 && !w.getIsFloating())
            windowsList.push_back(w.getDrawable());

    // hack
    xcb_window_t* ArrWindowList = &windowsList[0];

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_CLIENT_LIST"], XCB_ATOM_WINDOW,
        32, windowsList.size(), ArrWindowList);

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_CLIENT_LIST_STACKING"], XCB_ATOM_WINDOW,
        32, windowsList.size(), ArrWindowList);
}

void EWMH::refreshAllExtents() {
    for (auto& w : g_pWindowManager->windows)
        if (w.getDrawable() > 0)
	    setFrameExtents(w.getDrawable());
}

void EWMH::setFrameExtents(xcb_window_t w) {
    const auto BORDERSIZE = ConfigManager::getInt("border_size");
    uint32_t extents[4] = {BORDERSIZE,BORDERSIZE,BORDERSIZE,BORDERSIZE};
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, w, HYPRATOMS["_NET_FRAME_EXTENTS"], XCB_ATOM_CARDINAL, 32, 4, &extents);
}

void EWMH::updateDesktops() {
    
    int ACTIVEWORKSPACE = -1;

    if (!g_pWindowManager->getMonitorFromCursor()) {
        Debug::log(ERR, "Monitor was null! (updateDesktops EWMH) Using LastWindow");
        if (const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(g_pWindowManager->LastWindow); PWINDOW)
            ACTIVEWORKSPACE = g_pWindowManager->activeWorkspaces[PWINDOW->getWorkspaceID()] - 1; // because xorg counts from zero
        else
            ACTIVEWORKSPACE = 0;
    } else {
       ACTIVEWORKSPACE = g_pWindowManager->activeWorkspaces[g_pWindowManager->getMonitorFromCursor()->ID] - 1;
    }
    
    if (DesktopInfo::lastid != ACTIVEWORKSPACE) {
        // Update the current workspace
        DesktopInfo::lastid = ACTIVEWORKSPACE;
        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_CURRENT_DESKTOP"], XCB_ATOM_CARDINAL, 32, 1, &ACTIVEWORKSPACE);


        // Update all desktops
        const auto ALLDESKTOPS = g_pWindowManager->workspaces.size();
        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_NUMBER_OF_DESKTOPS"], XCB_ATOM_CARDINAL, 32, 1, &ALLDESKTOPS);

        // Update desktop names, create a sorted workspaces vec
        auto workspacesVec = g_pWindowManager->workspaces;
        std::sort(workspacesVec.begin(), workspacesVec.end(), [](CWorkspace& a, CWorkspace& b) { return a.getID() < b.getID(); });

        int msglen = 0;
        for (auto& work : workspacesVec) {
            if (work.getID() == SCRATCHPAD_ID)
                continue;
            msglen += strlen(std::to_string(work.getID()).c_str()) + 1;
        }

        char names[msglen];
        int curpos = 0;
        for (auto& work : workspacesVec) {
            for (int i = 0; i < strlen(std::to_string(work.getID()).c_str()) + 1; ++i) {
                if (work.getID() == SCRATCHPAD_ID)
                    break;
                    
                names[curpos] = std::to_string(work.getID())[i];
                ++curpos;
            }
        }

        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_DESKTOP_NAMES"], HYPRATOMS["UTF8_STRING"], 8, msglen, names);
    
        // Also update where the workspaces are so that bars and shit can read which monitor they belong to.
        uint32_t workspaceCoords[ALLDESKTOPS * 2];

        int pos = 0;
        for (int i = 0; i < ALLDESKTOPS; ++i) {
            workspaceCoords[pos++] = g_pWindowManager->monitors[g_pWindowManager->workspaces[i].getMonitor()].vecPosition.x;
            workspaceCoords[pos++] = g_pWindowManager->monitors[g_pWindowManager->workspaces[i].getMonitor()].vecPosition.y;
        }

        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, g_pWindowManager->Screen->root, HYPRATOMS["_NET_DESKTOP_VIEWPORT"], XCB_ATOM_CARDINAL, 32, pos, &workspaceCoords);
    }
}

void EWMH::updateWindow(xcb_window_t win) {
    const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(win);

    if (!PWINDOW || win < 1)
        return;

    const auto WORKSPACE = PWINDOW->getWorkspaceID();
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, win, HYPRATOMS["_NET_WM_DESKTOP"], XCB_ATOM_CARDINAL, 32, 1, &WORKSPACE);

    // ICCCM State Normal
    if (!PWINDOW->getDock()) {
        long data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, win, HYPRATOMS["WM_STATE"], HYPRATOMS["WM_STATE"], 32, 2, data);

        if (PWINDOW->getDrawable() == g_pWindowManager->LastWindow) {
            uint32_t dataa[] = {HYPRATOMS["_NET_WM_STATE_FOCUSED"]};
            xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_APPEND, PWINDOW->getDrawable(), HYPRATOMS["_NET_WM_STATE"], XCB_ATOM_ATOM, 32, 1, dataa);
        } else {
            removeAtom(PWINDOW->getDrawable(), HYPRATOMS["_NET_WM_STATE"], HYPRATOMS["_NET_WM_STATE_FOCUSED"]);
        }
    }
}

void EWMH::checkTransient(xcb_window_t window) {

    const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(window);

    if (!PWINDOW)
        return;

    // Check if it's a transient
    const auto TRANSIENTCOOKIE = xcb_get_property(g_pWindowManager->DisplayConnection, false, window, 68 /* TRANSIENT_FOR */, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
    const auto TRANSIENTREPLY = xcb_get_property_reply(g_pWindowManager->DisplayConnection, TRANSIENTCOOKIE, NULL);

    if (!TRANSIENTREPLY || xcb_get_property_value_length(TRANSIENTREPLY) == 0) {
        Debug::log(WARN, "Transient check failed.");
        return;
    }

    xcb_window_t transientWindow;
    if (!xcb_icccm_get_wm_transient_for_from_reply(&transientWindow, TRANSIENTREPLY)) {
        Debug::log(WARN, "Transient reply failed.");
        free(TRANSIENTREPLY);
        return;
    }

    // set the flags
    const auto PPARENTWINDOW = g_pWindowManager->getWindowFromDrawable(transientWindow);

    if (!PPARENTWINDOW) {
        free(TRANSIENTREPLY);
        Debug::log(LOG, "Transient set for a nonexistent window, ignoring.");
        return;
    }

    PPARENTWINDOW->addTransientChild(window);

    Debug::log(LOG, "Added a transient child to " + std::to_string(transientWindow) + ".");

    free(TRANSIENTREPLY);
}