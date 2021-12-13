#pragma once

#include "../defines.hpp"

namespace EWMH {
    void            setupInitEWMH();
    void            updateCurrentWindow(xcb_window_t);
    void            updateClientList();

    inline xcb_window_t EWMHwindow = 0;
};  