#pragma once

#include "../defines.hpp"

class CWorkspace {
public:
    EXPOSED_MEMBER(ID, int, i);
    EXPOSED_MEMBER(LastWindow, xcb_drawable_t, i);
};
