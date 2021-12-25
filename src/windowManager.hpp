#pragma once

#include "defines.hpp"
#include "window.hpp"

#include <vector>
#include <thread>
#include <xcb/xcb.h>
#include <deque>

#include "KeybindManager.hpp"
#include "utilities/Workspace.hpp"
#include "config/ConfigManager.hpp"
#include "utilities/Monitor.hpp"
#include "utilities/Util.hpp"
#include "utilities/AnimationUtil.hpp"
#include "utilities/XCBProps.hpp"
#include "ewmh/ewmh.hpp"
#include "bar/Bar.hpp"
#include "utilities/Tray.hpp"

#include "ipc/ipc.hpp"

class CWindowManager {
public:
    xcb_connection_t*           DisplayConnection = nullptr;
    xcb_ewmh_connection_t*      EWMHConnection = nullptr; // Bar uses this
    xcb_screen_t*               Screen = nullptr;
    xcb_drawable_t              Drawable;
    int                         RandREventBase = -1;
    uint32_t                    Values[3];

    std::vector<SMonitor>       monitors;

    bool                        modKeyDown = false;
    int                         mouseKeyDown = 0;
    Vector2D                    mouseLastPos = Vector2D(0, 0);
    int64_t                     actingOnWindowFloating = 0;

    uint8_t                     Depth = 32;
    xcb_visualtype_t*           VisualType;
    xcb_colormap_t              Colormap;

    std::deque<CWindow>         windows; // windows never left. It has always been hiding amongst us.
    std::deque<CWindow>         unmappedWindows;
    xcb_drawable_t              LastWindow = -1;

    std::deque<CWorkspace>      workspaces;
    std::deque<int>             activeWorkspaces;

    // Pipes
    SIPCPipe                    m_sIPCBarPipeIn = {ISDEBUG ? "/tmp/hypr/hyprbarind" : "/tmp/hypr/hyprbarin", 0};
    SIPCPipe                    m_sIPCBarPipeOut = {ISDEBUG ? "/tmp/hypr/hyprbaroutd" : "/tmp/hypr/hyprbarout", 0};
    CStatusBar*                 statusBar = nullptr;
    Vector2D                    lastKnownBarPosition = {-1,-1};
    int64_t                     barWindowID = 0;
    GThread*                    barThread; /* Well right now anything but the bar but lol */
    
    std::deque<CTrayClient>     trayclients;

    bool                        mainThreadBusy = false;
    bool                        animationUtilBusy = false;

    xcb_cursor_t                pointerCursor;
    xcb_cursor_context_t*       pointerContext;

    CWindow*                    getWindowFromDrawable(int64_t);
    void                        addWindowToVectorSafe(CWindow);
    void                        removeWindowFromVectorSafe(int64_t);

    void                        setupManager();
    bool                        handleEvent();
    void                        recieveEvent();
    void                        refreshDirtyWindows();

    void                        setFocusedWindow(xcb_drawable_t);
    void                        refocusWindowOnClosed();

    void                        calculateNewWindowParams(CWindow*);
    void                        fixWindowOnClose(CWindow*);
    void                        closeWindowAllChecks(int64_t);

    void                        moveActiveWindowTo(char);
    void                        moveActiveFocusTo(char);
    void                        moveActiveWindowToWorkspace(int);
    void                        warpCursorTo(Vector2D);
    void                        toggleWindowFullscrenn(const int&);
    void                        recalcAllDocks();

    void                        changeWorkspaceByID(int);
    void                        setAllWorkspaceWindowsDirtyByID(int);
    int                         getHighestWorkspaceID();
    CWorkspace*                 getWorkspaceByID(int);
    bool                        isWorkspaceVisible(int workspaceID);

    void                        setAllWindowsDirty();
    void                        setAllFloatingWindowsTop();
    void                        setAWindowTop(xcb_window_t);

    SMonitor*                   getMonitorFromWindow(CWindow*);
    SMonitor*                   getMonitorFromCursor();

    Vector2D                    getCursorPos();

    // finds a window that's tiled at cursor.
    CWindow*                    findWindowAtCursor();

    CWindow*                    findFirstWindowOnWorkspace(const int&);

    bool                        shouldBeFloatedOnInit(int64_t);
    void                        doPostCreationChecks(CWindow*);
    void                        getICCCMWMProtocols(CWindow*);

    void                        setupRandrMonitors();
    void                        createAndOpenAllPipes();
    void                        setupDepth();
    void                        setupColormapAndStuff();

    void                        updateActiveWindowName();
    void                        updateBarInfo();

    int                         getWindowsOnWorkspace(const int&);
    CWindow*                    getFullscreenWindowByWorkspace(const int&);

    void                        recalcAllWorkspaces();

    void                        moveWindowToUnmapped(int64_t);
    void                        moveWindowToMapped(int64_t);
    bool                        isWindowUnmapped(int64_t);

    void                        setAllWorkspaceWindowsAboveFullscreen(const int&);
    void                        setAllWorkspaceWindowsUnderFullscreen(const int&);

    void                        handleClientMessage(xcb_client_message_event_t*);

private:

    // Internal WM functions that don't have to be exposed

    void                        sanityCheckOnWorkspace(int);
    CWindow*                    getNeighborInDir(char dir);
    void                        eatWindow(CWindow* a, CWindow* toEat);
    bool                        canEatWindow(CWindow* a, CWindow* toEat);
    bool                        isNeighbor(CWindow* a, CWindow* b);
    void                        calculateNewTileSetOldTile(CWindow* pWindow);
    void                        calculateNewFloatingWindow(CWindow* pWindow);
    void                        setEffectiveSizePosUsingConfig(CWindow* pWindow);
    void                        cleanupUnusedWorkspaces();
    xcb_visualtype_t*           setupColors(const int&);
    void                        updateRootCursor();
    void                        applyShapeToWindow(CWindow* pWindow);
    SMonitor*                   getMonitorFromWorkspace(const int&);
    void                        recalcEntireWorkspace(const int&);
    void                        fixMasterWorkspaceOnClosed(CWindow* pWindow);
    void                        startWipeAnimOnWorkspace(const int&, const int&);
    void                        focusOnWorkspace(const int&);
};

inline std::unique_ptr<CWindowManager> g_pWindowManager = std::make_unique<CWindowManager>();

inline std::map<std::string, xcb_atom_t> HYPRATOMS = {
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
    HYPRATOM("_NET_REQUEST_FRAME_EXTENTS"),
    HYPRATOM("_NET_FRAME_EXTENTS"),
    HYPRATOM("_MOTIF_WM_HINTS"),
    HYPRATOM("WM_CHANGE_STATE"),
    HYPRATOM("_NET_SYSTEM_TRAY_OPCODE"),
    HYPRATOM("_NET_SYSTEM_TRAY_COLORS"),
    HYPRATOM("_NET_SYSTEM_TRAY_VISUAL"),
    HYPRATOM("_NET_SYSTEM_TRAY_ORIENTATION"),
    HYPRATOM("_XEMBED_INFO"),
    HYPRATOM("MANAGER")};
