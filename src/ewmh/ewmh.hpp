#pragma once

#include "../defines.hpp"

namespace EWMH {
    void            setupInitEWMH();
    void            updateCurrentWindow(xcb_window_t);
    void            updateWindow(xcb_window_t);
    void            updateClientList();
    void            setFrameExtents(xcb_window_t);
    void 	        refreshAllExtents();
    void            updateDesktops();
    void            checkTransient(xcb_window_t);

    namespace DesktopInfo {
        inline int lastid = 0;
    };

    inline xcb_window_t EWMHwindow = XCB_WINDOW_NONE;
};  
