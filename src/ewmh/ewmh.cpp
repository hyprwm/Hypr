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

    Debug::log(LOG, "EWMH init done.");
}