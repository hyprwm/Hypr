#include <stdlib.h>
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

#include <memory>
#include <string>
#include <algorithm>
#include <map>

#include "./helpers/Vector.hpp"
#include "./utilities/Debug.hpp"

#define EXPOSED_MEMBER(var, type, prefix) \
    private: \
        type m_##prefix##var; \
    public: \
        type get##var() { return this->m_##prefix##var; } \
        void set##var(type value) { this->m_##prefix##var = value; }


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


#define VECTORDELTANONZERO(veca, vecb) ((int)abs(veca.x - vecb.x) > 0 || (int)abs(veca.y - vecb.y) > 0)

#define PROP(cookie, name, len) const auto cookie = xcb_get_property(DisplayConnection, false, window, name, XCB_GET_PROPERTY_TYPE_ANY, 0, len); \
        const auto cookie##reply = xcb_get_property_reply(DisplayConnection, cookie, NULL)



#define HYPRATOM(name) {name, 0}

