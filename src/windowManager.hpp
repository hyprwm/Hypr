#pragma once

#include "defines.hpp"
#include "window.hpp"

#include <vector>


// temp config values
#define BORDERSIZE 1
#define GAPS_IN 10
#define GAPS_OUT 20

namespace WindowManager {
    inline xcb_connection_t*    DisplayConnection;
    inline xcb_screen_t*        Screen;
    inline xcb_drawable_t       Drawable;
    inline uint32_t             Values[3];

    inline std::vector<CWindow> windows; // windows never left. It has always been hiding amongst us.
    inline xcb_drawable_t       LastWindow = -1;

    CWindow*                    getWindowFromDrawable(xcb_drawable_t);
    void                        addWindowToVectorSafe(CWindow);
    void                        removeWindowFromVectorSafe(xcb_drawable_t);

    void                        setupManager();
    bool                        handleEvent();
    void                        refreshDirtyWindows();

    void                        setFocusedWindow(xcb_drawable_t);

    void                        calculateNewWindowParams(CWindow*);
    void                        fixWindowOnClose(CWindow*);
};