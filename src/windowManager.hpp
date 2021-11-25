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
#include "utilities/AnimationUtil.hpp"
#include "utilities/XCBProps.hpp"
#include "ewmh/ewmh.hpp"

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

    std::atomic<bool>           mainThreadBusy = false;
    std::atomic<bool>           animationUtilBusy = false;

    CWindow*                    getWindowFromDrawable(int64_t);
    void                        addWindowToVectorSafe(CWindow);
    void                        removeWindowFromVectorSafe(int64_t);

    void                        setupManager();
    bool                        handleEvent();
    void                        refreshDirtyWindows();

    void                        setFocusedWindow(xcb_drawable_t);

    void                        calculateNewWindowParams(CWindow*);
    void                        fixWindowOnClose(CWindow*);
    void                        closeWindowAllChecks(int64_t);

    void                        moveActiveWindowTo(char);
    void                        moveActiveWindowToWorkspace(int);
    void                        warpCursorTo(Vector2D);

    void                        changeWorkspaceByID(int);
    void                        setAllWorkspaceWindowsDirtyByID(int);
    int                         getHighestWorkspaceID();
    CWorkspace*                 getWorkspaceByID(int);
    bool                        isWorkspaceVisible(int workspaceID);

    void                        setAllWindowsDirty();
    void                        setAllFloatingWindowsTop();

    SMonitor*                   getMonitorFromWindow(CWindow*);
    SMonitor*                   getMonitorFromCursor();

    Vector2D                    getCursorPos();

    // finds a window that's tiled at cursor.
    CWindow*                    findWindowAtCursor();

    bool                        shouldBeFloatedOnInit(int64_t);

   private:

    // Internal WM functions that don't have to be exposed

    void                        setupRandrMonitors();

    void                        sanityCheckOnWorkspace(int);
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

inline std::map<std::string, int64_t> HYPRATOMS = {
    HYPRATOM("_NET_SUPPORTED"),
    HYPRATOM("_NET_SUPPORTING_WM_CHECK"),
    HYPRATOM("_NET_WM_NAME"),
    HYPRATOM("_NET_WM_VISIBLE_NAME"),
    HYPRATOM("_NET_WM_MOVERESIZE"),
    HYPRATOM("_NET_WM_STATE_STICKY"),
    HYPRATOM("_NET_WM_STATE_FULLSCREEN"),
    HYPRATOM("_NET_WM_STATE_DEMANDS_ATTENTION"),
    HYPRATOM("_NET_WM_STATE_MODAL"),
    HYPRATOM("_NET_WM_STATE_HIDDEN"),
    HYPRATOM("_NET_WM_STATE_FOCUSED"),
    HYPRATOM("_NET_WM_STATE"),
    HYPRATOM("_NET_WM_WINDOW_TYPE"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NORMAL"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DOCK"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DIALOG"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_UTILITY"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLBAR"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_SPLASH"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_POPUP_MENU"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_TOOLTIP"),
    HYPRATOM("_NET_WM_WINDOW_TYPE_NOTIFICATION"),
    HYPRATOM("_NET_WM_DESKTOP"),
    HYPRATOM("_NET_WM_STRUT_PARTIAL"),
    HYPRATOM("_NET_CLIENT_LIST"),
    HYPRATOM("_NET_CLIENT_LIST_STACKING"),
    HYPRATOM("_NET_CURRENT_DESKTOP"),
    HYPRATOM("_NET_NUMBER_OF_DESKTOPS"),
    HYPRATOM("_NET_DESKTOP_NAMES"),
    HYPRATOM("_NET_DESKTOP_VIEWPORT"),
    HYPRATOM("_NET_ACTIVE_WINDOW"),
    HYPRATOM("_NET_CLOSE_WINDOW"),
    HYPRATOM("_NET_MOVERESIZE_WINDOW"),
    HYPRATOM("_NET_WM_USER_TIME"),
    HYPRATOM("_NET_STARTUP_ID"),
    HYPRATOM("_NET_WORKAREA"),
    HYPRATOM("_NET_WM_ICON"),
    HYPRATOM("WM_PROTOCOLS"),
    HYPRATOM("WM_DELETE_WINDOW"),
    HYPRATOM("UTF8_STRING"),
    HYPRATOM("WM_STATE"),
    HYPRATOM("WM_CLIENT_LEADER"),
    HYPRATOM("WM_TAKE_FOCUS"),
    HYPRATOM("WM_WINDOW_ROLE"),
    HYPRATOM("I3_SOCKET_PATH"),
    HYPRATOM("I3_CONFIG_PATH"),
    HYPRATOM("I3_SYNC"),
    HYPRATOM("I3_SHMLOG_PATH"),
    HYPRATOM("I3_PID"),
    HYPRATOM("I3_LOG_STREAM_SOCKET_PATH"),
    HYPRATOM("I3_FLOATING_WINDOW"),
    HYPRATOM("_NET_REQUEST_FRAME_EXTENTS"),
    HYPRATOM("_NET_FRAME_EXTENTS"),
    HYPRATOM("_MOTIF_WM_HINTS"),
    HYPRATOM("WM_CHANGE_STATE"),
    HYPRATOM("MANAGER")
};
