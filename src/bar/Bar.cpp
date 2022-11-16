#include "Bar.hpp"

#include <codecvt>
#include <locale>
#include "../config/ConfigManager.hpp"
#include "../windowManager.hpp"

bool isParentDead() {
    const auto PPID = getppid();

    return PPID == 1;
}

void parseEvent() {
    while(1) {
        g_pWindowManager->recieveEvent();
    }
}

int64_t barMainThread() {
    // Main already created all the pipes

    Debug::log(LOG, "Child says Hello World!");

    // Well now this is the init
    // it's pretty tricky because we only need to init the stuff we need
    g_pWindowManager->DisplayConnection = xcb_connect(NULL, &barScreen);
    if (const auto RET = xcb_connection_has_error(g_pWindowManager->DisplayConnection); RET != 0) {
        Debug::log(CRIT, "Connection Failed! Return: " + std::to_string(RET));
        return RET;
    }

    // Screen
    g_pWindowManager->Screen = xcb_aux_get_screen(g_pWindowManager->DisplayConnection, barScreen);

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

    // Start the parse event thread
    std::thread([=]() {
        parseEvent();
    }).detach();

    // Init config manager
    ConfigManager::init();

    if (ConfigManager::getInt("bar:enabled") == 1) {
        Debug::log(LOG, "Bar enabled, reload config to launch it.");
        ConfigManager::loadConfigLoadVars();
    }

    Debug::log(LOG, "Bar setup finished!");

    int lazyUpdateCounter = 0;

    // setup the tray so apps send to us
    if (!STATUSBAR.getIsDestroyed())
        STATUSBAR.setupTray();

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

    // delete it from the heap
    delete module;
}

void CStatusBar::setupTray() {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return;

    Debug::log(LOG, "Setting up tray!");

    char atomName[strlen("_NET_SYSTEM_TRAY_S") + 11];

    snprintf(atomName, strlen("_NET_SYSTEM_TRAY_S") + 11, "_NET_SYSTEM_TRAY_S%d", barScreen);

    // init the atom
    const auto TRAYCOOKIE = xcb_intern_atom(g_pWindowManager->DisplayConnection, 0, strlen(atomName), atomName);

    trayWindowID = xcb_generate_id(g_pWindowManager->DisplayConnection);

    uint32_t values[] = {g_pWindowManager->Screen->black_pixel, g_pWindowManager->Screen->black_pixel, 1, g_pWindowManager->Colormap};

    xcb_create_window(g_pWindowManager->DisplayConnection, g_pWindowManager->Depth, trayWindowID,
                      g_pWindowManager->Screen->root, -1, -1, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, g_pWindowManager->VisualType->visual_id,
                      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP, 
                      values);

    xcb_atom_t dockAtom[] = {HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"]};
    xcb_ewmh_set_wm_window_type(g_pWindowManager->EWMHConnection, trayWindowID, 1, dockAtom);

    const uint32_t ORIENTATION = 0; // Horizontal
    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, trayWindowID,
                        HYPRATOMS["_NET_SYSTEM_TRAY_ORIENTATION"], XCB_ATOM_CARDINAL,
                        32, 1, &ORIENTATION);

    xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, trayWindowID,
                        HYPRATOMS["_NET_SYSTEM_TRAY_VISUAL"], XCB_ATOM_VISUALID,
                        32, 1, &g_pWindowManager->VisualType->visual_id);

    // COLORS

    // Check if the tray module is active
    SBarModule* pBarModule = nullptr;
    for (auto& mod : modules) {
        if (mod->value == "tray") {
            pBarModule = mod;
            break;
        }
    }

    if (pBarModule) {
        // init colors
        const auto R = (uint16_t)(RED(pBarModule->bgcolor) * 255.f);
        const auto G = (uint16_t)(GREEN(pBarModule->bgcolor) * 255.f);
        const auto B = (uint16_t)(BLUE(pBarModule->bgcolor) * 255.f);

        const unsigned short TRAYCOLORS[] = {
            R, G, B, R, G, B, R, G, B, R, G, B // Foreground, Error, Warning, Success
        };

        xcb_change_property(g_pWindowManager->DisplayConnection, XCB_PROP_MODE_REPLACE, trayWindowID,
                            HYPRATOMS["_NET_SYSTEM_TRAY_COLORS"], XCB_ATOM_CARDINAL, 32, 12, TRAYCOLORS);
    }

    //

    const auto TRAYREPLY = xcb_intern_atom_reply(g_pWindowManager->DisplayConnection, TRAYCOOKIE, NULL);

    if (!TRAYREPLY) {
        Debug::log(ERR, "Tray reply NULL! Aborting tray...");
        free(TRAYREPLY);
        return;
    }

    // set the owner and check
    xcb_set_selection_owner(g_pWindowManager->DisplayConnection, trayWindowID, TRAYREPLY->atom, XCB_CURRENT_TIME);

    const auto SELCOOKIE = xcb_get_selection_owner(g_pWindowManager->DisplayConnection, TRAYREPLY->atom);
    const auto SELREPLY = xcb_get_selection_owner_reply(g_pWindowManager->DisplayConnection, SELCOOKIE, NULL);

    if (!SELREPLY) {
        Debug::log(ERR, "Selection owner reply NULL! Aborting tray...");
        free(SELREPLY);
        free(TRAYREPLY);
        return;
    }

    if (SELREPLY->owner != trayWindowID) {
        Debug::log(ERR, "Couldn't set the Tray owner, maybe a different tray is running??");
        free(SELREPLY);
        free(TRAYREPLY);
        return;
    }

    free(SELREPLY);
    free(TRAYREPLY);

    Debug::log(LOG, "Tray setup done, sending message!");

    uint8_t buf[32] = {NULL};
    xcb_client_message_event_t* event = (xcb_client_message_event_t*)buf;

    event->response_type = XCB_CLIENT_MESSAGE;
    event->window = g_pWindowManager->Screen->root;
    event->type = HYPRATOMS["MANAGER"];
    event->format = 32;
    event->data.data32[0] = 0L;
    event->data.data32[1] = TRAYREPLY->atom;
    event->data.data32[2] = trayWindowID;

    xcb_send_event(g_pWindowManager->DisplayConnection, 0, g_pWindowManager->Screen->root, 0xFFFFFF, (char*)buf);

    Debug::log(LOG, "Tray message sent!");
}

void CStatusBar::fixTrayOnCreate() {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return;

    if (m_bHasTray && ConfigManager::getInt("bar:no_tray_saving") == 0) {
        for (auto& tray : g_pWindowManager->trayclients) {
            xcb_reparent_window(g_pWindowManager->DisplayConnection, tray.window, g_pWindowManager->statusBar->trayWindowID, 0, 0);
            xcb_map_window(g_pWindowManager->DisplayConnection, tray.window);
            tray.hidden = false;
        }
    } else {
        uint32_t values[2];

        values[0] = 0;
        values[1] = 0;

        for (auto& tray : g_pWindowManager->trayclients) {
            tray.hidden = true;
            values[0] = 0;
            values[1] = 0;
            xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
            values[0] = 30000;
            values[1] = 30000;
            xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        }
    }
}

void CStatusBar::saveTrayOnDestroy() {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return;

    // TODO: fix this instead of disabling it.

    if (ConfigManager::getInt("bar:no_tray_saving") == 1)
        return;

    for (auto& tray : g_pWindowManager->trayclients) {
        xcb_reparent_window(g_pWindowManager->DisplayConnection, tray.window, g_pWindowManager->Screen->root, 30000, 30000);
    }
}

void CStatusBar::setup(int MonitorID) {
    Debug::log(LOG, "Creating the bar!");

    if (MonitorID > g_pWindowManager->monitors.size()) {
        MonitorID = 0;
        Debug::log(ERR, "Incorrect value in MonitorID for the bar. Setting to 0.");
    }

    m_bHasTray = false;
    for (auto& mod : g_pWindowManager->statusBar->modules) {
        if (mod->value == "tray") {
            m_bHasTray = true;
            break;
        }
    }

    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        m_bHasTray = false;

    const auto MONITOR = g_pWindowManager->monitors[MonitorID];

    Debug::log(LOG, "Bar monitor found to be " + std::to_string(MONITOR.ID));

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
    Debug::log(LOG, "Bar mapping!");

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

    // fix tray
    fixTrayOnCreate();

    Debug::log(LOG, "Bar setup done!");

    m_bIsDestroyed = false;
}

void CStatusBar::destroy() {
    Debug::log(LOG, "Destroying the bar!");

    if (m_bIsDestroyed) 
        return;

    saveTrayOnDestroy();

    xcb_close_font(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].Font);
    xcb_unmap_window(g_pWindowManager->DisplayConnection, m_iWindowID);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iWindowID);
    xcb_destroy_window(g_pWindowManager->DisplayConnection, m_iPixmap);

    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["BG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["MEDBG"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["TEXT"].GContext);
    xcb_free_gc(g_pWindowManager->DisplayConnection, m_mContexts["HITEXT"].GContext);

    // Free cairo
    cairo_destroy(m_pCairo);
    m_pCairo = nullptr;

    m_bIsDestroyed = true;
}

int CStatusBar::getTextWidth(std::string text, std::string font, double size) {
    cairo_select_font_face(m_pCairo, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, size);

    cairo_text_extents_t textextents;
    cairo_text_extents(m_pCairo, text.c_str(), &textextents);
   
    return textextents.x_advance;
}

void CStatusBar::drawText(Vector2D pos, std::string text, uint32_t color, std::string font, double size) {
    cairo_select_font_face(m_pCairo, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_pCairo, size);
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

void CStatusBar::drawErrorScreen() {
    if (ConfigManager::getInt("autogenerated") == 1)
        drawCairoRectangle(Vector2D(0, 0), m_vecSize, 0xffffff33);
    else
        drawCairoRectangle(Vector2D(0, 0), m_vecSize, 0xFFaa1111);
        
    drawText(Vector2D(1, getTextHalfY()), ConfigManager::parseError, 0xff000000,
        ConfigManager::getString("bar:font.main"), ConfigManager::getFloat("bar:font.size"));

    // do all the drawing cuz we return later
    cairo_surface_flush(m_pCairoSurface);

    xcb_copy_area(g_pWindowManager->DisplayConnection, m_iPixmap, m_iWindowID, m_mContexts["BG"].GContext,
                  0, 0, 0, 0, m_vecSize.x, m_vecSize.y);

    xcb_flush(g_pWindowManager->DisplayConnection);
}

void CStatusBar::draw() {

    if (m_bIsDestroyed)
        return;

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

    if (ConfigManager::parseError != "") {
        // draw a special error screen
        drawErrorScreen();
        return;
    }

    drawCairoRectangle(Vector2D(0, 0), m_vecSize, ConfigManager::getInt("bar:col.bg"));

    //
    //
    // DRAW ALL MODULES

    int offLeft = 0;
    int offRight = 0;

    for (auto& module : modules) {

        if (!module->bgcontext && !module->isPad)
            setupModule(module);

        if (module->value == "workspaces") {
            offLeft += drawWorkspacesModule(module, offLeft);
        } else {
            if (module->alignment == LEFT) {
                offLeft += drawModule(module, offLeft);
            } else if (module->alignment == RIGHT) {
                offRight += drawModule(module, offRight);
            } else {
                drawModule(module, 0);
            }
        }
    }

    //
    //
    //

    // fix the fucking tray
    if (!m_bHasTray) {
        uint32_t values[2];
        for (auto& tray : g_pWindowManager->trayclients) {
            tray.hidden = true;
            values[0] = 30000;
            values[1] = 30000;
            xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        }
    }


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

        drawText(Vector2D(off + m_vecSize.y * drawnWorkspaces + m_vecSize.y / 2.f - getTextWidth(workspaceName, ConfigManager::getString("bar:font.main"), ConfigManager::getFloat("bar:font.size")) / 2.f, getTextHalfY()),
                 workspaceName, WORKSPACE == MOUSEWORKSPACEID ? ConfigManager::getInt("bar:col.font_secondary") : ConfigManager::getInt("bar:col.font_main"), ConfigManager::getString("bar:font.main"), ConfigManager::getFloat("bar:font.size"));

        drawnWorkspaces++;
    }

    return drawnWorkspaces * m_vecSize.y;
}

int CStatusBar::drawTrayModule(SBarModule* mod, int off) {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return 0;

    const auto PAD = 2;

    const auto ELEMENTWIDTH = (m_vecSize.y - 2 < 1 ? 1 : m_vecSize.y - 2);

    const auto MODULEWIDTH = g_pWindowManager->trayclients.size() * (ELEMENTWIDTH + PAD);

    Vector2D position;
    switch (mod->alignment) {
        case LEFT:
            position = Vector2D(off, 0);
            break;
        case RIGHT:
            position = Vector2D(m_vecSize.x - off - MODULEWIDTH, 0);
            break;
        case CENTER:
            position = Vector2D(m_vecSize.x / 2.f - (MODULEWIDTH) / 2.f, 0);
            break;
    }

    // draw tray

    if (MODULEWIDTH < 1)
        return 0;

    drawCairoRectangle(position, Vector2D(MODULEWIDTH, m_vecSize.y), mod->bgcolor);

    int i = 0;
    for (auto& tray : g_pWindowManager->trayclients) {
        uint32_t values[] = {(int)(position.x + (i * (ELEMENTWIDTH + PAD)) + PAD / 2.f), (int)position.y + 1, (int)XCB_STACK_MODE_ABOVE};

        if (tray.hidden) {
            values[0] = -999;
            values[1] = -999;
            xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window,
                                 XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE, values);
            continue;
        }

        xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE, values);

        
        // fix the size
        values[0] = ELEMENTWIDTH;
        values[1] = values[0];

        xcb_configure_window(g_pWindowManager->DisplayConnection, tray.window,
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

        ++i;
    }

    return MODULEWIDTH;
}

int CStatusBar::drawModule(SBarModule* mod, int off) {

    if (mod->isPad)
        return mod->pad;

    if (mod->value == "tray")
        return drawTrayModule(mod, off);

    const int PAD = ConfigManager::getInt("bar:mod_pad_in");

    // check if we need to update
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - mod->updateLast).count() > mod->updateEveryMs) {
        // This is done asynchronously to prevent lag on especially slower PCs
        // but ngl it also did hang on more powerful ones
        std::thread([=](){
            const auto RETVAL = BarCommands::parseCommand(mod->value);
            mod->accessValueCalculated(true, RETVAL);
            mod->updateLast = std::chrono::system_clock::now();
        }).detach();
    }

    // We have the value, draw the module!

    const auto MODULEWIDTH = getTextWidth(mod->valueCalculated, ConfigManager::getString("bar:font.main"), ConfigManager::getFloat("bar:font.size")) + PAD;
    const auto ICONWIDTH = getTextWidth(mod->icon, ConfigManager::getString("bar:font.secondary"), ConfigManager::getFloat("bar:font.size"));

    if (!MODULEWIDTH || mod->accessValueCalculated(false) == "")
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

    drawText(position + Vector2D(PAD / 2, getTextHalfY()), mod->icon, mod->color,
        ConfigManager::getString("bar:font.secondary"), ConfigManager::getFloat("bar:font.size"));
    drawText(position + Vector2D(PAD / 2 + ICONWIDTH, getTextHalfY()), mod->accessValueCalculated(false), mod->color,
        ConfigManager::getString("bar:font.main"), ConfigManager::getFloat("bar:font.size"));

    return MODULEWIDTH + ICONWIDTH;
}

void CStatusBar::ensureTrayClientDead(xcb_window_t window) {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return;

    auto temp = g_pWindowManager->trayclients;

    g_pWindowManager->trayclients.clear();

    for (auto& trayitem : temp) {
        if (trayitem.window != window)
            g_pWindowManager->trayclients.push_back(trayitem);
    }

    Debug::log(LOG, "Ensured client dead (Bar, Tray)");
}

void CStatusBar::ensureTrayClientHidden(xcb_window_t window, bool hide) {
    if (ConfigManager::getInt("bar:force_no_tray") == 1)
        return;

    for (auto& trayitem : g_pWindowManager->trayclients) {
        if (trayitem.window == window)
            trayitem.hidden = hide;
    }
}