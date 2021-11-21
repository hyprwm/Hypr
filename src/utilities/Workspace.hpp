#pragma once

#include "../defines.hpp"

class CWorkspace {
public:
    CWorkspace();
    ~CWorkspace();

    EXPOSED_MEMBER(ID, int, i);
    EXPOSED_MEMBER(LastWindow, xcb_drawable_t, i);

    EXPOSED_MEMBER(Monitor, int, i);

    EXPOSED_MEMBER(HasFullscreenWindow, bool, b);
};
