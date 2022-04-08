#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xinerama.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_cursor.h>
#include <xcb/shape.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <glib-2.0/glib.h>

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <regex>
#include <vector>
#include <filesystem>

#include "./helpers/Vector.hpp"
#include "./utilities/Debug.hpp"

#ifndef NDEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif

// hints
#define NONMOVABLE
#define NONCOPYABLE
//

#define EXPOSED_MEMBER(var, type, prefix) \
    private: \
        type m_##prefix##var; \
    public: \
        inline type get##var() { return m_##prefix##var; } \
        void set##var(type value) { m_##prefix##var = value; }


#define EVENT(name) \
    void event##name(xcb_generic_event_t* event);

#define STICKS(a, b) abs((a) - (b)) < 2

#define VECINRECT(vec, x1, y1, x2, y2) (vec.x >= (x1) && vec.x <= (x2) && vec.y >= (y1) && vec.y <= (y2))

#define XCBQUERYCHECK(name, query, errormsg) \
    xcb_generic_error_t* error##name;        \
    const auto name = query;                 \
                                             \
    if (error##name != NULL) {               \
        Debug::log(ERR, errormsg);           \
        free(error##name);                   \
        free(name);                          \
        return;                              \
    }                                        \
    free(error##name);


#define VECTORDELTANONZERO(veca, vecb) (abs(veca.x - vecb.x) > 0.4f || abs(veca.y - vecb.y) > 0.4f)
#define VECTORDELTAMORETHAN(veca, vecb, delta) (abs(veca.x - vecb.x) > (delta) || abs(veca.y - vecb.y) > (delta))

#define PROP(cookie, name, len) const auto cookie = xcb_get_property(DisplayConnection, false, window, name, XCB_GET_PROPERTY_TYPE_ANY, 0, len); \
        const auto cookie##reply = xcb_get_property_reply(DisplayConnection, cookie, NULL)



#define HYPRATOM(name) {name, 0}

#define ALPHA(c) ((double)(((c) >> 24) & 0xff) / 255.0)
#define RED(c) ((double)(((c) >> 16) & 0xff) / 255.0)
#define GREEN(c) ((double)(((c) >> 8) & 0xff) / 255.0)
#define BLUE(c) ((double)(((c)) & 0xff) / 255.0)

#define CONTAINS(s, f) s.find(f) != std::string::npos

#define RETURNIFBAR  if (g_pWindowManager->statusBar) return;
#define RETURNIFMAIN if (!g_pWindowManager->statusBar) return;

#define COLORDELTAOVERX(c, c1, d) (abs(RED(c) - RED(c1)) > d / 255.f || abs(GREEN(c) - GREEN(c1)) > d / 255.f || abs(BLUE(c) - BLUE(c1)) > d / 255.f || abs(ALPHA(c) - ALPHA(c1)) > d / 255.f)

#define _NET_MOVERESIZE_WINDOW_X (1 << 8)
#define _NET_MOVERESIZE_WINDOW_Y (1 << 9)
#define _NET_MOVERESIZE_WINDOW_WIDTH (1 << 10)
#define _NET_MOVERESIZE_WINDOW_HEIGHT (1 << 11)

#define SCRATCHPAD_ID 1337420