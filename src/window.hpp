#pragma once

#include "defines.hpp"
#include "utilities/Workspace.hpp"

class CWindow {
public:
    CWindow();
    ~CWindow();

    void        move(Vector2D dest);
    void        moveByDelta(Vector2D delta);

    void        resize(Vector2D size);
    void        resize(float percx, float percy);

    std::string getName();

    // Tells the window manager to reload the window's params
    EXPOSED_MEMBER(Dirty, bool, b);

    EXPOSED_MEMBER(Size, Vector2D, vec);
    EXPOSED_MEMBER(EffectiveSize, Vector2D, vec);
    EXPOSED_MEMBER(EffectivePosition, Vector2D, vec);
    EXPOSED_MEMBER(Position, Vector2D, vec);
    EXPOSED_MEMBER(IsFloating, bool, b);
    EXPOSED_MEMBER(Drawable, xcb_drawable_t, i);

    // Workspace pointer
    EXPOSED_MEMBER(WorkspaceID, int, i);
    

private:

};