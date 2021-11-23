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
    exposeEvent.window = g_pWindowManager->statusBar.getWindowID();
    exposeEvent.response_type = 0;
    exposeEvent.x = 0;
    exposeEvent.y = 0;
    exposeEvent.width = g_pWindowManager->Screen->width_in_pixels;
    exposeEvent.height = g_pWindowManager->Screen->height_in_pixels;
    xcb_send_event(g_pWindowManager->DisplayConnection, false, g_pWindowManager->Screen->root, XCB_EVENT_MASK_EXPOSURE, (char*)&exposeEvent);
    xcb_flush(g_pWindowManager->DisplayConnection);
}