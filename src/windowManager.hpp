#pragma once

#include "defines.hpp"
#include "window.hpp"

#include <vector>
#include <thread>
#include <xcb/xcb.h>

#include "KeybindManager.hpp"
#include "utilities/Workspace.hpp"
#include "bar/Bar.hpp"


// temp config values
#define BORDERSIZE 1
#define GAPS_IN 5
#define GAPS_OUT 20
#define BAR_HEIGHT 15

#define MAX_FPS 60

class CWindowManager {
public:
    xcb_connection_t*           DisplayConnection;
    xcb_screen_t*               Screen;
    xcb_drawable_t              Drawable;
    uint32_t                    Values[3];

    uint8_t                     Depth = 32;
    xcb_visualtype_t*           VisualType;

    std::vector<CWindow>        windows; // windows never left. It has always been hiding amongst us.
    xcb_drawable_t              LastWindow = -1;

    std::vector<CWorkspace>     workspaces;
    CWorkspace*                 activeWorkspace = nullptr;

    CStatusBar                  statusBar;

    CWindow*                    getWindowFromDrawable(xcb_drawable_t);
    void                        addWindowToVectorSafe(CWindow);
    void                        removeWindowFromVectorSafe(xcb_drawable_t);

    void                        setupManager();
    bool                        handleEvent();
    void                        refreshDirtyWindows();

    void                        setFocusedWindow(xcb_drawable_t);

    void                        calculateNewWindowParams(CWindow*);
    void                        fixWindowOnClose(CWindow*);

    void                        moveActiveWindowTo(char);
    void                        warpCursorTo(Vector2D);

    void                        changeWorkspaceByID(int);
    void                        setAllWorkspaceWindowsDirtyByID(int);
    int                         getHighestWorkspaceID();
    CWorkspace*                 getWorkspaceByID(int);

private:

    // Internal WM functions that don't have to be exposed

    CWindow*                    getNeighborInDir(char dir);
    void                        eatWindow(CWindow* a, CWindow* toEat);
    bool                        canEatWindow(CWindow* a, CWindow* toEat);
    bool                        isNeighbor(CWindow* a, CWindow* b);
    void                        calculateNewTileSetOldTile(CWindow* pWindow);
    void                        setEffectiveSizePosUsingConfig(CWindow* pWindow);
    void                        cleanupUnusedWorkspaces();
    xcb_visualtype_t*           setupColors();

};

inline std::unique_ptr<CWindowManager> g_pWindowManager = std::make_unique<CWindowManager>();
