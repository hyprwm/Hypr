#include "Bar.hpp"

#include <codecvt>
#include <locale>

#include "../windowManager.hpp"

void CStatusBar::setup(Vector2D origin, Vector2D size) {
    m_vecPosition = origin;
    m_vecSize = size;

    // Create a pixmap for writing to.
    m_iPixmap = xcb_generate_id(g_pWindowManager->DisplayConnection);
    xcb_create_pixmap(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, m_iPixmap, m_iWindowID, m_vecSize.x, m_vecSize.y);

    // setup contexts.. ugh.
    uint32_t values[4];

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
    return WIDTH;
}

void CStatusBar::draw() {

    xcb_rectangle_t rectangles[] = {{m_vecPosition.x, m_vecPosition.y, m_vecSize.x + m_vecPosition.x, m_vecPosition.y + m_vecSize.y}};
    xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, m_mContexts["BG"].GContext, 1, rectangles);

    // Draw workspaces
    int drawnWorkspaces = 0;
    for (int i = 0; i <= g_pWindowManager->getHighestWorkspaceID(); ++i) {

        const auto WORKSPACE = g_pWindowManager->getWorkspaceByID(i);

        if (!WORKSPACE)
            continue;

        std::string workspaceName = std::to_string(i);

        if (WORKSPACE->getID() == g_pWindowManager->activeWorkspace->getID()) {
            xcb_rectangle_t rectangleActive[] = { { m_vecSize.y * drawnWorkspaces, 0, m_vecSize.y, m_vecSize.y } };
            xcb_poly_fill_rectangle(g_pWindowManager->DisplayConnection, m_iPixmap, m_mContexts["MEDBG"].GContext, 1, rectangleActive);
        }

        xcb_image_text_8(g_pWindowManager->DisplayConnection, workspaceName.length(), m_iPixmap,
                         WORKSPACE->getID() == g_pWindowManager->activeWorkspace->getID() ? m_mContexts["HITEXT"].GContext : m_mContexts["BASETEXT"].GContext,
                         m_vecSize.y * drawnWorkspaces + m_vecSize.y / 2.f - 2, m_vecSize.y - (m_vecSize.y - 10) / 2, workspaceName.c_str());

        drawnWorkspaces++;
    }

    // Draw time to the right
    const auto TIMET = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::string TIME = "Hello World!";
    xcb_image_text_8(g_pWindowManager->DisplayConnection, TIME.length(), m_iPixmap,
                     m_mContexts["BASETEXT"].GContext, m_vecSize.x - getTextWidth(TIME, m_mContexts["BASETEXT"].Font) - 2, (m_vecSize.y - (m_vecSize.y - 10) / 2),
                     TIME.c_str());

    xcb_flush(g_pWindowManager->DisplayConnection);

    xcb_copy_area(g_pWindowManager->DisplayConnection, m_iPixmap, m_iWindowID, m_mContexts["BG"].GContext, 
        0, 0, 0, 0, m_vecSize.x, m_vecSize.y);
}