#include <inttypes.h>

#include <thread>

#include "../windowManager.hpp"

namespace Events {
    EVENT(Enter);
    EVENT(Leave);
    EVENT(Destroy);
    EVENT(MapWindow);
    EVENT(ButtonPress);
    EVENT(Expose);
    EVENT(KeyPress);

    // A thread to notify xcb to redraw our shiz
    void            redraw();
    void            setThread();

    inline          timer_t timerid;
};