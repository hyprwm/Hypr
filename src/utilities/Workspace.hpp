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

    // Wipe animations
    EXPOSED_MEMBER(AnimationInProgress, bool, b);
    EXPOSED_MEMBER(CurrentOffset, Vector2D, vec);
    EXPOSED_MEMBER(GoalOffset, Vector2D, vec);
};
