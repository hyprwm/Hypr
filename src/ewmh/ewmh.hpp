#pragma once

#include "../defines.hpp"

namespace EWMH {
    void            setupInitEWMH();
    void            updateCurrentWindow(xcb_window_t);
    void            updateClientList();
    void            setFrameExtents(xcb_window_t);
    void 	    refreshAllExtents();

    inline xcb_window_t EWMHwindow = XCB_WINDOW_NONE;
};  
