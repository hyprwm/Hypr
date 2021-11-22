#pragma once

#include "defines.hpp"
#include "window.hpp"

#include <vector>
#include <thread>
#include <xcb/xcb.h>

#include "KeybindManager.hpp"
#include "utilities/Workspace.hpp"
#include "bar/Bar.hpp"
#include "config/ConfigManager.hpp"
#include "utilities/Monitor.hpp"
#include "utilities/Util.hpp"

class CWindowManager {
public:
    xcb_connection_t*           DisplayConnection;
    xcb_screen_t*               Screen;
    xcb_drawable_t              Drawable;
    uint32_t                    Values[3];

    std::vector<SMonitor>       monitors;

    bool                        modKeyDown = false;
    int                         mouseKeyDown = 0;
    Vector2D                    mouseLastPos = Vector2D(0, 0);
    int64_t                     actingOnWindowFloating = 0;

    uint8_t                     Depth = 32;
    xcb_visualtype_t*           VisualType;

    std::vector<CWindow>        windows; // windows never left. It has always been hiding amongst us.
    xcb_drawable_t              LastWindow = -1;

    std::vector<CWorkspace>     workspaces;
    std::vector<int>            activeWorkspaces;

    CStatusBar                  statusBar;
    std::thread*                barThread;

    CWindow*                    getWindowFromDrawable(int64_t);
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
    bool                        isWorkspaceVisible(int workspaceID);

    void                        setAllWindowsDirty();

    SMonitor*                   getMonitorFromWindow(CWindow*);
    SMonitor*                   getMonitorFromCursor();

    Vector2D                    getCursorPos();

    // finds a window that's tiled at cursor.
    CWindow*                    findWindowAtCursor();

   private:

    // Internal WM functions that don't have to be exposed

    void                        setupRandrMonitors();

    CWindow*                    getNeighborInDir(char dir);
    void                        eatWindow(CWindow* a, CWindow* toEat);
    bool                        canEatWindow(CWindow* a, CWindow* toEat);
    bool                        isNeighbor(CWindow* a, CWindow* b);
    void                        calculateNewTileSetOldTile(CWindow* pWindow);
    void                        calculateNewFloatingWindow(CWindow* pWindow);
    void                        setEffectiveSizePosUsingConfig(CWindow* pWindow);
    void                        cleanupUnusedWorkspaces();
    xcb_visualtype_t*           setupColors();
    void                        updateBarInfo();

};

inline std::unique_ptr<CWindowManager> g_pWindowManager = std::make_unique<CWindowManager>();
