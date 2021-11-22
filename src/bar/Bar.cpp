#include "Bar.hpp"

#include <codecvt>
#include <locale>

#include "../windowManager.hpp"

void CStatusBar::setup(int MonitorID) {

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

    values[0] = 0x111111;
    values[1] = 0x111111;
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBG->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    //
    //

    auto contextBASETEXT = &m_mContexts["BASETEXT"];

    contextBASETEXT->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);
    contextBASETEXT->Font = xcb_generate_id(g_pWindowManager->DisplayConnection);
    xcb_open_font(g_pWindowManager->DisplayConnection, contextBASETEXT->Font, 5, "fixed");
    values[0] = 0xFFFFFF;
    values[1] = 0x111111;
    values[2] = contextBASETEXT->Font;

    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBASETEXT->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND | XCB_GC_FONT, values);

    //
    //

    auto contextHITEXT = &m_mContexts["HITEXT"];
    contextHITEXT->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);
    contextHITEXT->Font = contextBASETEXT->Font;
    values[0] = 0x000000;
    values[1] = 0xFF3333;
    values[2] = contextHITEXT->Font;

    xcb_create_gc(g_pWindowManager->DisplayConnection, contextHITEXT->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND | XCB_GC_FONT, values);

    //
    //

    auto contextMEDBG = &m_mContexts["MEDBG"];
    contextMEDBG->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);

    values[0] = 0xFF3333;
    values[1] = 0x111111;
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextMEDBG->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    // don't, i use it later
    //xcb_close_font(g_pWindowManager->DisplayConnection, contextBASETEXT->Font);


    // Set the bar to be top
    //values[0] = XCB_STACK_MODE_ABOVE;
    //xcb_configure_window(g_pWindowManager->DisplayConnection, m_iWindowID, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

void CStatusBar::destroy() {
    xcb_close_font(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].Font);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iWindowID);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iPixmap);

    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["BG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["MEDBG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["TEXT"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].GContext);
}

int getTextWidth(std::string text, xcb_font_t font) {

    // conv from utf8 to UCS-2 (what the fuck Xorg why)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;
    std::wstring wideString = strCnv.from_bytes(text);

    // create a xcb string
    xcb_char2b_t bytes[wideString.length()];

    for (int i = 0; i < wideString.length(); ++i) {
        bytes[i].byte1 = 0x0; // Only ASCII support. TODO: Maybe more?
        bytes[i].byte2 = wideString[i] & 0xFF;
    }

    xcb_generic_error_t* error;
    const auto COOKIE = xcb_query_text_extents(g_pWindowManager->DisplayConnection, font, wideString.length() - 1, (xcb_char2b_t*)bytes);
    xcb_query_text_extents_reply_t* reply = xcb_query_text_extents_reply(g_pWindowManager->DisplayConnection, COOKIE, &error);
    if (!reply) {
        Debug::log(ERR, "Text extent failed, code " + std::to_string(error->error_code));
        free(error);
        return 0;
    }

    const auto WIDTH = reply->overall_width;
    free(reply);
    return WIDTH + 5;
}

void CStatusBar::draw() {

    const auto WORKSPACE = g_pWindowManager->getWorkspaceByID(g_pWindowManager->activeWorkspaces[m_iMonitorID]);

    if (WORKSPACE->getHasFullscreenWindow())
        return; // Do not draw a bar on a fullscreen window.

    // TODO: CRIT! Status bar flashes, workspaces are wonky (wrong IDs?)

    xcb_rectangle_t rectangles[] = {{(int)0, (int)0, (int)m_vecSize.x, (int)m_vecSize.y}};
    xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, m_mContexts["BG"].GContext, 1, rectangles);

    // Draw workspaces
    int drawnWorkspaces = 0;
    for (int i = 0; i < openWorkspaces.size(); ++i) {

        const auto WORKSPACE = openWorkspaces[i];

        // The LastWindow may be on a different one. This is where the mouse is.
        const auto MOUSEWORKSPACEID = m_iCurrentWorkspace;

        if (!WORKSPACE)
            continue;

        std::string workspaceName = std::to_string(openWorkspaces[i]);

        if (WORKSPACE == MOUSEWORKSPACEID) {
            xcb_rectangle_t rectangleActive[] = { { m_vecSize.y * drawnWorkspaces, 0, m_vecSize.y, m_vecSize.y } };
            xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, m_mContexts["MEDBG"].GContext, 1, rectangleActive);
        }

        xcb_image_text_8(g_pWindowManager->DisplayConnection, workspaceName.length(), m_iPixmap,
                         WORKSPACE == MOUSEWORKSPACEID ? m_mContexts["HITEXT"].GContext : m_mContexts["BASETEXT"].GContext,
                         m_vecSize.y * drawnWorkspaces + m_vecSize.y / 2.f - (WORKSPACE > 9 ? 4 : 2), m_vecSize.y - (m_vecSize.y - 10) / 2, workspaceName.c_str());

        drawnWorkspaces++;
    }

    // Draw time to the right
    std::string TIME = exec("date +%I:%M\\ %p");
    TIME = TIME.substr(0, TIME.length() - 1);
    xcb_image_text_8(g_pWindowManager->DisplayConnection, TIME.length(), m_iPixmap,
                     m_mContexts["BASETEXT"].GContext, m_vecSize.x - getTextWidth(TIME, m_mContexts["BASETEXT"].Font), (m_vecSize.y - (m_vecSize.y - 10) / 2),
                     TIME.c_str());

    xcb_flush(g_pWindowManager->DisplayConnection);

    xcb_copy_area(g_pWindowManager->DisplayConnection, m_iPixmap, m_iWindowID, m_mContexts["BG"].GContext, 
        0, 0, 0, 0, m_vecSize.x, m_vecSize.y);
}