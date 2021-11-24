#pragma once

#include "../defines.hpp"

namespace EWMH {
    void            setupInitEWMH();

    inline xcb_window_t EWMHwindow = 0;
};  