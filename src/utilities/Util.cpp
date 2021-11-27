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
    const char* const ENVHOME = getenv("HOME");
    const std::string DEBUGPATH = ENVHOME + (std::string) "/.hypr.log";
    logs.open(DEBUGPATH, std::ios::out | std::ios::trunc);
    logs << " ";
    logs.close();
}

double parabolic(double from, double to, double incline) {
    return from + ((to - from) / incline);
}

void emptyEvent() {
    xcb_expose_event_t exposeEvent;
    exposeEvent.window = 0;
    exposeEvent.response_type = 0;
    exposeEvent.x = 0;
    exposeEvent.y = 0;
    exposeEvent.width = g_pWindowManager->Screen->width_in_pixels;
    exposeEvent.height = g_pWindowManager->Screen->height_in_pixels;
    xcb_send_event(g_pWindowManager->DisplayConnection, false, g_pWindowManager->Screen->root, XCB_EVENT_MASK_EXPOSURE, (char*)&exposeEvent);
    xcb_flush(g_pWindowManager->DisplayConnection);
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