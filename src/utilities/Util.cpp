#include "Util.hpp"
#include "../windowManager.hpp"

// Execute a shell command and get the output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "Exec failed in pipe.");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void clearLogs() {
    std::ofstream logs;
    const std::string DEBUGPATH = "/tmp/hypr/hypr.log";
    const std::string DEBUGPATH2 = "/tmp/hypr/hyprd.log";
    unlink(DEBUGPATH2.c_str());
    unlink(DEBUGPATH.c_str());
}

double parabolic(double from, double to, double incline) {
    return from + ((to - from) / incline);
}

CFloatingColor parabolicColor(CFloatingColor from, uint32_t to, double incline) {
    from.r = parabolic(from.r, RED(to) * 255.f, incline);
    from.g = parabolic(from.g, GREEN(to) * 255.f, incline);
    from.b = parabolic(from.b, BLUE(to) * 255.f, incline);
    from.a = parabolic(from.a, ALPHA(to) * 255.f, incline);

    return from;
}

CFloatingColor parabolicColor(CFloatingColor from, CFloatingColor to, double incline) {
    from.r = parabolic(from.r, to.r, incline);
    from.g = parabolic(from.g, to.g, incline);
    from.b = parabolic(from.b, to.b, incline);
    from.a = parabolic(from.a, to.a, incline);

    return from;
}

void emptyEvent(xcb_drawable_t window) {
    xcb_expose_event_t exposeEvent;
    exposeEvent.window = window;
    exposeEvent.response_type = 0;
    exposeEvent.x = 0;
    exposeEvent.y = 0;
    exposeEvent.width = g_pWindowManager->Screen->width_in_pixels;
    exposeEvent.height = g_pWindowManager->Screen->height_in_pixels;
    xcb_send_event(g_pWindowManager->DisplayConnection, false, window, XCB_EVENT_MASK_EXPOSURE, (char*)&exposeEvent);
    xcb_flush(g_pWindowManager->DisplayConnection);
}

void wakeUpEvent(xcb_drawable_t window) {
    const auto PWINDOW = g_pWindowManager->getWindowFromDrawable(window);

    if (!PWINDOW)
        return;

    PWINDOW->setRealPosition(PWINDOW->getRealPosition() + Vector2D(1, 1));
    PWINDOW->setDirty(true);

    g_pWindowManager->refreshDirtyWindows();

    xcb_flush(g_pWindowManager->DisplayConnection);

    PWINDOW->setRealPosition(PWINDOW->getRealPosition() - Vector2D(1, 1));
    PWINDOW->setDirty(true);
}

bool xcbContainsAtom(xcb_get_property_reply_t* PROP, xcb_atom_t ATOM) {
    if (PROP == NULL || xcb_get_property_value_length(PROP) == 0)
        return false;

    const auto ATOMS = (xcb_atom_t*)xcb_get_property_value(PROP);
    if (!ATOMS)
        return false;

    for (int i = 0; i < xcb_get_property_value_length(PROP) / (PROP->format / 8); ++i)
        if (ATOMS[i] == ATOM)
            return true;

    return false;
}

std::vector<std::string> splitString(std::string in, char c) {
    std::vector<std::string> returns;

    while(in.length() > 0) {
        std::string toPush = in.substr(0, in.find_first_of(c));
        if (toPush != "") {
            returns.push_back(toPush);
        }
        
        if (in.find_first_of(c) != std::string::npos)
            in = in.substr(in.find_first_of(c) + 1);
        else
            in = "";
    }

    return returns;
}