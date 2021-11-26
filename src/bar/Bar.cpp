#include "Bar.hpp"

#include <codecvt>
#include <locale>

#include "../windowManager.hpp"

void CStatusBar::setup(int MonitorID) {
    Debug::log(LOG, "Creating the bar!");

    if (MonitorID > g_pWindowManager->monitors.size()) {
        MonitorID = 0;
        Debug::log(ERR, "Incorrect value in MonitorID for the bar. Setting to 0.");
    }
        
    const auto MONITOR = g_pWindowManager->monitors[MonitorID];

    m_iMonitorID = MonitorID;
    m_vecPosition = MONITOR.vecPosition;
    m_vecSize = Vector2D(MONITOR.vecSize.x, ConfigManager::getInt("bar_height"));

    uint32_t values[4];

    // window
    m_iWindowID = (xcb_generate_id(g_pWindowManager->DisplayConnection));

    values[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;

    xcb_create_window(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, m_iWindowID,
                      g_pWindowManager->Screen->root, m_vecPosition.x, m_vecPosition.y, m_vecSize.x, m_vecSize.y,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, g_pWindowManager->Screen->root_visual,
                      XCB_CW_EVENT_MASK, values);

    // map
    xcb_map_window(g_pWindowManager->DisplayConnection, m_iWindowID);

    // Create a pixmap for writing to.
    m_iPixmap = xcb_generate_id(g_pWindowManager->DisplayConnection);
    xcb_create_pixmap(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, m_iPixmap, m_iWindowID, m_vecSize.x, m_vecSize.y);

    // setup contexts.. ugh..

    auto contextBG = &m_mContexts["BG"];
    contextBG->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);

    values[0] = 0xFF111111;
    values[1] = 0xFF111111;
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBG->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    //
    //

    auto contextBGT = &m_mContexts["BGT"];
    contextBGT->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);

    values[0] = 0x00000000;
    values[1] = 0x00000000;
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBGT->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    //
    //

    auto contextBASETEXT = &m_mContexts["BASETEXT"];

    contextBASETEXT->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);
    contextBASETEXT->Font = xcb_generate_id(g_pWindowManager->DisplayConnection);
    xcb_open_font(g_pWindowManager->DisplayConnection, contextBASETEXT->Font, 5, "fixed");
    values[0] = 0xFFFFFFFF;
    values[1] = 0xFF111111;
    values[2] = contextBASETEXT->Font;

    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBASETEXT->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND | XCB_GC_FONT, values);

    //
    //

    auto contextHITEXT = &m_mContexts["HITEXT"];
    contextHITEXT->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);
    contextHITEXT->Font = contextBASETEXT->Font;
    values[0] = 0xFF000000;
    values[1] = 0xFFFF3333;
    values[2] = contextHITEXT->Font;

    xcb_create_gc(g_pWindowManager->DisplayConnection, contextHITEXT->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND | XCB_GC_FONT, values);

    //
    //

    auto contextMEDBG = &m_mContexts["MEDBG"];
    contextMEDBG->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);

    values[0] = 0xFFFF3333;
    values[1] = 0xFF111111;
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextMEDBG->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    // don't, i use it later
    //xcb_close_font(g_pWindowManager->DisplayConnection, contextBASETEXT->Font);

    m_pCairoSurface = cairo_xcb_surface_create(g_pWindowManager->DisplayConnection, m_iPixmap, g_pWindowManager->VisualType,
                                               m_vecSize.x, m_vecSize.y);
    m_pCairo = cairo_create(m_pCairoSurface);
    cairo_surface_destroy(m_pCairoSurface);
}

void CStatusBar::destroy() {
    Debug::log(LOG, "Destroying the bar!");

    xcb_close_font(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].Font);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iWindowID);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iPixmap);

    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["BG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["MEDBG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["TEXT"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].GContext);

    // Free cairo
    cairo_destroy(m_pCairo);
    m_pCairo = nullptr;
}

int CStatusBar::getTextWidth(std::string text) {
    cairo_select_font_face(m_pCairo, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, 12);

    cairo_text_extents_t textextents;
    cairo_text_extents(m_pCairo, text.c_str(), &textextents);
   
    return textextents.width + 1 /* pad */;
}

void CStatusBar::drawText(Vector2D pos, std::string text, uint32_t color) {
    cairo_select_font_face(m_pCairo, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, 12);
    cairo_set_source_rgba(m_pCairo, RED(color), GREEN(color), BLUE(color), ALPHA(color));
    cairo_move_to(m_pCairo, pos.x, pos.y);
    cairo_show_text(m_pCairo, text.c_str());
}

void CStatusBar::draw() {

    const auto WORKSPACE = g_pWindowManager->getWorkspaceByID(g_pWindowManager->activeWorkspaces[m_iMonitorID]);

    if (!WORKSPACE || WORKSPACE->getHasFullscreenWindow()) // TODO: fix this
        return; // Do not draw a bar on a fullscreen window.

    if (!m_pCairo) {
        Debug::log(ERR, "Cairo is null but attempted to draw!");
        return;
    }

    xcb_rectangle_t rectangles[] = {{(int)0, (int)0, (int)m_vecSize.x, (int)m_vecSize.y}};
    xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, m_mContexts["BG"].GContext, 1, rectangles);

    // Draw workspaces
    int drawnWorkspaces = 0;
    for (long unsigned int i = 0; i < openWorkspaces.size(); ++i) {

        const auto WORKSPACE = openWorkspaces[i];

        // The LastWindow may be on a different one. This is where the mouse is.
        const auto MOUSEWORKSPACEID = m_iCurrentWorkspace;

        if (!WORKSPACE)
            continue;

        std::string workspaceName = std::to_string(openWorkspaces[i]);

        xcb_rectangle_t rectangleActive[] = { { m_vecSize.y * drawnWorkspaces, 0, m_vecSize.y, m_vecSize.y } };
        xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, WORKSPACE == MOUSEWORKSPACEID ? m_mContexts["MEDBG"].GContext : m_mContexts["BG"].GContext, 1, rectangleActive);

        drawText(Vector2D(m_vecSize.y * drawnWorkspaces + m_vecSize.y / 2.f - getTextWidth(workspaceName) / 2.f, m_vecSize.y - (m_vecSize.y - 9) / 2.f),
                 workspaceName, WORKSPACE == MOUSEWORKSPACEID ? 0xFF111111 : 0xFFFFFFFF);

        drawnWorkspaces++;
    }

    // Draw STATUS to the right
    std::string STATUS = exec(m_szStatusCommand.c_str());
    STATUS = STATUS.substr(0, (STATUS.length() > 0 ? STATUS.length() - 1 : 9999999));
    if (STATUS != "") {
        drawText(Vector2D(m_vecSize.x - getTextWidth(STATUS), m_vecSize.y - (m_vecSize.y - 9) / 2.f),
                 STATUS, 0xFFFFFFFF);
    }
    

    cairo_surface_flush(m_pCairoSurface);

    // clear before copying
    //xcb_clear_area(g_pWindowManager->DisplayConnection, 0, m_iWindowID, 0, 0, m_vecSize.x, m_vecSize.y);
    //xcb_flush(g_pWindowManager->DisplayConnection);

    xcb_copy_area(g_pWindowManager->DisplayConnection, m_iPixmap, m_iWindowID, m_mContexts["BG"].GContext, 
        0, 0, 0, 0, m_vecSize.x, m_vecSize.y);

    xcb_flush(g_pWindowManager->DisplayConnection);
}