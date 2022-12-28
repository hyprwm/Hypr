#include <inttypes.h>

#include <thread>

#include "../windowManager.hpp"

namespace Events {
    EVENT(Enter);
    EVENT(Leave);
    EVENT(Destroy);
    EVENT(MapWindow);
    EVENT(UnmapWindow);
    EVENT(ButtonPress);
    EVENT(ButtonRelease);
    EVENT(Expose);
    EVENT(KeyPress);
    EVENT(MotionNotify);
    EVENT(ClientMessage);
    EVENT(Configure);

    EVENT(RandRScreenChange);

    // Bypass some events for floating windows
    CWindow*        remapWindow(int, bool floating = false, int forcemonitor = -1);
    CWindow*        remapFloatingWindow(int, int forcemonitor = -1);

    // A thread to notify xcb to redraw our shiz
    void            redraw();
    void            setThread();

    // For docks etc
    inline bool     nextWindowCentered = false;

    // Fix focus on open
    inline std::deque<uint64_t> ignoredEvents;

    // Fix spammed RandR events
    inline std::chrono::high_resolution_clock::time_point lastRandREvent = std::chrono::high_resolution_clock::now();
    inline int      susRandREventNo = 0;
};