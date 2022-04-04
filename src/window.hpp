#pragma once

#include "defines.hpp"
#include "utilities/Workspace.hpp"
#include "utilities/Util.hpp"

class CWindow {
public:
    CWindow();
    ~CWindow();

    void        move(Vector2D dest);
    void        moveByDelta(Vector2D delta);

    void        resize(Vector2D size);
    void        resize(float percx, float percy);

    // ------------------------------------- //
    //              Node Stuff               //
    // ------------------------------------- //

    // IDs:
    // > 0 : Windows
    // == 0 : None
    // < 0 : Nodes

    EXPOSED_MEMBER(ParentNodeID, int64_t, i);

    EXPOSED_MEMBER(ChildNodeAID, int64_t, i);
    EXPOSED_MEMBER(ChildNodeBID, int64_t, i);

    void        generateNodeID();

    // ------------------------------------- //

    EXPOSED_MEMBER(Name, std::string, sz);
    EXPOSED_MEMBER(ClassName, std::string, sz);
    EXPOSED_MEMBER(RoleName, std::string, sz);
    EXPOSED_MEMBER(Constructed, bool, b);
    EXPOSED_MEMBER(FirstOpen, bool, b);

    // Tells the window manager to reload the window's params
    EXPOSED_MEMBER(Dirty, bool, b);

    void        bringTopRecursiveTransients();
    void        setDirtyRecursive(bool);
    void        addTransientChild(xcb_drawable_t);
    // ONLY for dwindle layout!
    void        recalcSizePosRecursive();

    EXPOSED_MEMBER(Size, Vector2D, vec);
    EXPOSED_MEMBER(EffectiveSize, Vector2D, vec);
    EXPOSED_MEMBER(EffectivePosition, Vector2D, vec);
    EXPOSED_MEMBER(Position, Vector2D, vec);
    EXPOSED_MEMBER(RealSize, Vector2D, vec);
    EXPOSED_MEMBER(RealPosition, Vector2D, vec);
    EXPOSED_MEMBER(LastUpdateSize, Vector2D, vec);
    EXPOSED_MEMBER(LastUpdatePosition, Vector2D, vec);
    EXPOSED_MEMBER(IsFloating, bool, b);
    EXPOSED_MEMBER(Drawable, int64_t, i);  // int64_t because it's my internal ID system too.

    // For splitting ratios
    EXPOSED_MEMBER(SplitRatio, float, f);

    // Fullscreen
    EXPOSED_MEMBER(Fullscreen, bool, b);

    // Workspace pointer
    EXPOSED_MEMBER(WorkspaceID, int, i);

    // For floating
    EXPOSED_MEMBER(DefaultSize, Vector2D, vec);
    EXPOSED_MEMBER(DefaultPosition, Vector2D, vec);
    EXPOSED_MEMBER(UnderFullscreen, bool, b);

    // Monitors
    EXPOSED_MEMBER(Monitor, int, i);

    // Docks etc
    EXPOSED_MEMBER(Immovable, bool, b);
    EXPOSED_MEMBER(NoInterventions, bool, b);

    // ICCCM
    EXPOSED_MEMBER(CanKill, bool, b);

    // Master layout
    EXPOSED_MEMBER(Master, bool, b);
    EXPOSED_MEMBER(MasterChildIndex, int, i);
    EXPOSED_MEMBER(Dead, bool, b);
    
    // Animating cheaply
    EXPOSED_MEMBER(IsAnimated, bool, b);
    EXPOSED_MEMBER(FirstAnimFrame, bool, b);

    // Weird shenaningans
    EXPOSED_MEMBER(IsSleeping, bool, b);

    // Animate borders
    EXPOSED_MEMBER(RealBorderColor, CFloatingColor, c);
    EXPOSED_MEMBER(EffectiveBorderColor, CFloatingColor, c);

    // Docks
    EXPOSED_MEMBER(Dock, bool, b);
    EXPOSED_MEMBER(DockAlign, EDockAlign, e);
    EXPOSED_MEMBER(DockHidden, bool, b);

    // Transient
    EXPOSED_MEMBER(Children, std::vector<int64_t>, vec);
    EXPOSED_MEMBER(Transient, bool, b);

    // Pseudotiling
    EXPOSED_MEMBER(IsPseudotiled, bool, b);
    EXPOSED_MEMBER(PseudoSize, Vector2D, vec);

    // For dragging tiled windows
    EXPOSED_MEMBER(DraggingTiled, bool, b);

    // For pinning floating
    EXPOSED_MEMBER(Pinned, bool, b);

private:

};