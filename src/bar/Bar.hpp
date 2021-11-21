#pragma once

#include <unordered_map>

#include "../defines.hpp"

struct SDrawingContext {
    xcb_gcontext_t      GContext;
    xcb_font_t          Font;
};

class CStatusBar {
public:

    EXPOSED_MEMBER(WindowID, xcb_window_t, i);

    void                draw();
    void                setup(Vector2D, Vector2D);
    void                destroy();

private:
    Vector2D            m_vecSize;
    Vector2D            m_vecPosition;

    xcb_pixmap_t        m_iPixmap;

    std::unordered_map<std::string, SDrawingContext> m_mContexts;
};