#include "Bar.hpp"

#include <codecvt>
#include <locale>
#include "../config/ConfigManager.hpp"
#include "../windowManager.hpp"

bool isParentDead() {
    const auto PPID = getppid();

    return PPID == 1;
}

int64_t barMainThread() {
    // Main already created all the pipes

    Debug::log(LOG, "Child says Hello World!");

    // Well now this is the init
    // it's pretty tricky because we only need to init the stuff we need
    g_pWindowManager->DisplayConnection = xcb_connect(NULL, NULL);
    if (const auto RET = xcb_connection_has_error(g_pWindowManager->DisplayConnection); RET != 0) {
        Debug::log(CRIT, "Connection Failed! Return: " + std::to_string(RET));
        return RET;
    }

    // Screen
    g_pWindowManager->Screen = xcb_setup_roots_iterator(xcb_get_setup(g_pWindowManager->DisplayConnection)).data;

    if (!g_pWindowManager->Screen) {
        Debug::log(CRIT, "Screen was null!");
        return 1;
    }

    Debug::log(LOG, "Bar init Phase 1 done.");

    // Init atoms
    // get atoms
    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(g_pWindowManager->DisplayConnection, 0, ATOM.first.length(), ATOM.first.c_str());
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(g_pWindowManager->DisplayConnection, cookie, NULL);

        if (!reply) {
            Debug::log(ERR, "Atom failed: " + ATOM.first);
            continue;
        }

        ATOM.second = reply->atom;
    }

    Debug::log(LOG, "Atoms done.");

    g_pWindowManager->EWMHConnection = (xcb_ewmh_connection_t*)malloc(sizeof(xcb_ewmh_connection_t));
    xcb_ewmh_init_atoms_replies(g_pWindowManager->EWMHConnection, xcb_ewmh_init_atoms(g_pWindowManager->DisplayConnection, g_pWindowManager->EWMHConnection), nullptr);

    Debug::log(LOG, "Bar init EWMH done.");

    // Init randr for monitors.
    g_pWindowManager->setupRandrMonitors();

    // Init depth
    g_pWindowManager->setupColormapAndStuff();

    // Setup our bar
    CStatusBar STATUSBAR;

    // Tell everyone we are in the child process.
    g_pWindowManager->statusBar = &STATUSBAR;

    Debug::log(LOG, "Bar init Phase 2 done.");

    // Init config manager
    ConfigManager::init();

    STATUSBAR.setup(0);

    Debug::log(LOG, "Bar setup finished!");

    int lazyUpdateCounter = 0;

    while (1) {

        // Don't spam these
        if (lazyUpdateCounter > 10) {
            ConfigManager::tick();

            lazyUpdateCounter = 0;
        }

        ++lazyUpdateCounter;

        // Recieve the message and send our reply
        IPCRecieveMessageB(g_pWindowManager->m_sIPCBarPipeIn.szPipeName);
        SIPCMessageBarToMain message;
        message.windowID = STATUSBAR.getWindowID();
        IPCSendMessage(g_pWindowManager->m_sIPCBarPipeOut.szPipeName, message);
        //

        // draw the bar
        STATUSBAR.draw();

        if (isParentDead()) {
            // Just for debugging
            SIPCMessageBarToMain message;
            message.windowID = 0;
            IPCSendMessage(g_pWindowManager->m_sIPCBarPipeOut.szPipeName, message);

            Debug::log(LOG, "Bar parent died!");

            return 0;  // If the parent died, kill the bar too.
        }
            

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / ConfigManager::getInt("max_fps")));
    }

    return 0;
}

void CStatusBar::setupModule(SBarModule* module) {
    uint32_t values[2];
    module->bgcontext = xcb_generate_id(g_pWindowManager->DisplayConnection);
    values[0] = module->bgcolor;
    values[1] = module->bgcolor;
    xcb_create_gc(g_pWindowManager->DisplayConnection, module->bgcontext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);
}

void CStatusBar::destroyModule(SBarModule* module) {
    if (module->bgcontext)
        xcb_free_gc(g_pWindowManager->DisplayConnection, module->bgcontext);
}

void CStatusBar::setup(int MonitorID) {
    Debug::log(LOG, "Creating the bar!");

    if (MonitorID > g_pWindowManager->monitors.size()) {
        MonitorID = 0;
        Debug::log(ERR, "Incorrect value in MonitorID for the bar. Setting to 0.");
    }
        
    const auto MONITOR = g_pWindowManager->monitors[MonitorID];

    m_iMonitorID = MonitorID;
    m_vecPosition = MONITOR.vecPosition;
    m_vecSize = Vector2D(MONITOR.vecSize.x, ConfigManager::getInt("bar:height"));

    uint32_t values[4];

    // window
    m_iWindowID = (xcb_generate_id(g_pWindowManager->DisplayConnection));

    // send the message IMMEDIATELY so that the main thread has time to update our WID.
    SIPCMessageBarToMain message;
    message.windowID = m_iWindowID;
    IPCSendMessage(g_pWindowManager->m_sIPCBarPipeOut.szPipeName, message);

    values[0] = ConfigManager::getInt("bar:col.bg");
    values[1] = ConfigManager::getInt("bar:col.bg");
    values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    values[3] = g_pWindowManager->Colormap;

    xcb_create_window(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, m_iWindowID,
                      g_pWindowManager->Screen->root, m_vecPosition.x, m_vecPosition.y, m_vecSize.x, m_vecSize.y,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, g_pWindowManager->VisualType->visual_id,
                      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, values);

    // Set the state to dock to avoid some issues
    xcb_atom_t dockAtom[] = { HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] };
    xcb_ewmh_set_wm_window_type(g_pWindowManager->EWMHConnection, m_iWindowID, 1, dockAtom);

    // map
    xcb_map_window(g_pWindowManager->DisplayConnection, m_iWindowID);

    // Create a pixmap for writing to.
    m_iPixmap = xcb_generate_id(g_pWindowManager->DisplayConnection);
    xcb_create_pixmap(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, m_iPixmap, m_iWindowID, m_vecSize.x, m_vecSize.y);

    // setup contexts

    auto contextBG = &m_mContexts["BG"];
    contextBG->GContext = xcb_generate_id(g_pWindowManager->DisplayConnection);

    values[0] = ConfigManager::getInt("bar:col.bg");
    values[1] = ConfigManager::getInt("bar:col.bg");
    xcb_create_gc(g_pWindowManager->DisplayConnection, contextBG->GContext, m_iPixmap, XCB_GC_BACKGROUND | XCB_GC_FOREGROUND, values);

    //
    //

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

int CStatusBar::getTextWidth(std::string text, std::string font) {
    cairo_select_font_face(m_pCairo, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, 12);

    cairo_text_extents_t textextents;
    cairo_text_extents(m_pCairo, text.c_str(), &textextents);
   
    return textextents.width + 1 /* pad */;
}

void CStatusBar::drawText(Vector2D pos, std::string text, uint32_t color, std::string font) {
    cairo_select_font_face(m_pCairo, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, 12);
    cairo_set_source_rgba(m_pCairo, RED(color), GREEN(color), BLUE(color), ALPHA(color));
    cairo_move_to(m_pCairo, pos.x, pos.y);
    cairo_show_text(m_pCairo, text.c_str());
}

void CStatusBar::drawCairoRectangle(Vector2D pos, Vector2D size, uint32_t color) {
    cairo_set_source_rgba(m_pCairo, RED(color), GREEN(color), BLUE(color), ALPHA(color));
    cairo_rectangle(m_pCairo, pos.x, pos.y, size.x, size.y);
    cairo_fill(m_pCairo);
}

int CStatusBar::getTextHalfY() {
    return m_vecSize.y - (m_vecSize.y - 9) / 2.f;
}

void CStatusBar::draw() {

    if (m_bIsCovered)
        return; // Do not draw a bar on a fullscreen window.

    if (!m_pCairo) {
        Debug::log(ERR, "Cairo is null but attempted to draw!");
        return;
    }

    // Clear the entire pixmap
    cairo_save(m_pCairo);
    cairo_set_operator(m_pCairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_pCairo);
    cairo_restore(m_pCairo);
    //

    drawCairoRectangle(Vector2D(0, 0), m_vecSize, ConfigManager::getInt("bar:col.bg"));

    //
    //
    // DRAW ALL MODULES

    int offLeft = 0;
    int offRight = 0;

    for (auto& module : modules) {

        if (!module.bgcontext && !module.isPad)
            setupModule(&module);

        if (module.value == "workspaces") {
            offLeft += drawWorkspacesModule(&module, offLeft);
        } else {
            if (module.alignment == LEFT) {
                offLeft += drawModule(&module, offLeft);
            } else if (module.alignment == RIGHT) {
                offRight += drawModule(&module, offRight);
            } else {
                drawModule(&module, 0);
            }
        }
    }

    //
    //
    //
    
    cairo_surface_flush(m_pCairoSurface);

    xcb_copy_area(g_pWindowManager->DisplayConnection, m_iPixmap, m_iWindowID, m_mContexts["BG"].GContext, 
        0, 0, 0, 0, m_vecSize.x, m_vecSize.y);

    xcb_flush(g_pWindowManager->DisplayConnection);
}

// Returns the width
int CStatusBar::drawWorkspacesModule(SBarModule* mod, int off) {
    // Draw workspaces
    int drawnWorkspaces = 0;
    for (long unsigned int i = 0; i < openWorkspaces.size(); ++i) {
        const auto WORKSPACE = openWorkspaces[i];

        // The LastWindow may be on a different one. This is where the mouse is.
        const auto MOUSEWORKSPACEID = m_iCurrentWorkspace;

        if (!WORKSPACE)
            continue;

        std::string workspaceName = std::to_string(openWorkspaces[i]);

        drawCairoRectangle(Vector2D(off + m_vecSize.y * drawnWorkspaces, 0), Vector2D(m_vecSize.y, m_vecSize.y), WORKSPACE == MOUSEWORKSPACEID ? ConfigManager::getInt("bar:col.high") : ConfigManager::getInt("bar:col.bg"));

        drawText(Vector2D(off + m_vecSize.y * drawnWorkspaces + m_vecSize.y / 2.f - getTextWidth(workspaceName, ConfigManager::getString("bar:font.main")) / 2.f, getTextHalfY()),
                 workspaceName, WORKSPACE == MOUSEWORKSPACEID ? 0xFF111111 : 0xFFFFFFFF, ConfigManager::getString("bar:font.main"));

        drawnWorkspaces++;
    }

    return drawnWorkspaces * m_vecSize.y;
}

int CStatusBar::drawModule(SBarModule* mod, int off) {

    if (mod->isPad)
        return mod->pad;

    const int PAD = ConfigManager::getInt("bar:mod_pad_in");

    // check if we need to update
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - mod->updateLast).count() > mod->updateEveryMs) {
        // Yes. Set the new last and do it.
        mod->updateLast = std::chrono::system_clock::now();

        mod->valueCalculated = BarCommands::parseCommand(mod->value);
    }

    // We have the value, draw the module!

    const auto MODULEWIDTH = getTextWidth(mod->valueCalculated, ConfigManager::getString("bar:font.main")) + PAD;
    const auto ICONWIDTH = getTextWidth(mod->icon, ConfigManager::getString("bar:font.secondary"));

    if (!MODULEWIDTH || mod->valueCalculated == "")
        return 0; // empty module

    Vector2D position;
    switch (mod->alignment) {
        case LEFT:
            position = Vector2D(off, 0);
            break;
        case RIGHT:
            position = Vector2D(m_vecSize.x - off - MODULEWIDTH - ICONWIDTH, 0);
            break;
        case CENTER:
            position = Vector2D(m_vecSize.x / 2.f - (MODULEWIDTH + ICONWIDTH) / 2.f, 0);
            break;
    }

    drawCairoRectangle(position, Vector2D(MODULEWIDTH + ICONWIDTH, m_vecSize.y), mod->bgcolor);

    drawText(position + Vector2D(PAD / 2, getTextHalfY()), mod->icon, mod->color, ConfigManager::getString("bar:font.secondary"));
    drawText(position + Vector2D(PAD / 2 + ICONWIDTH, getTextHalfY()), mod->valueCalculated, mod->color, ConfigManager::getString("bar:font.main"));

    return MODULEWIDTH + ICONWIDTH;
}