#pragma once

#include <unordered_map>

#include "../defines.hpp"
#include "../utilities/AnimationUtil.hpp"

struct SDrawingContext {
    xcb_gcontext_t      GContext;
    xcb_font_t          Font;
};

class CStatusBar {
public:

    EXPOSED_MEMBER(WindowID, xcb_window_t, i);
    EXPOSED_MEMBER(MonitorID, int, i);
    EXPOSED_MEMBER(StatusCommand, std::string, sz); // TODO: make the bar better

    void                draw();
    void                setup(int MonitorID);
    void                destroy();

    std::vector<int>    openWorkspaces;
    EXPOSED_MEMBER(CurrentWorkspace, int, i);

private:
    Vector2D            m_vecSize;
    Vector2D            m_vecPosition;

    xcb_pixmap_t        m_iPixmap;

    std::unordered_map<std::string, SDrawingContext> m_mContexts;
};