#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>

#include <memory>
#include <string>
#include <algorithm>

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
    xcb_generic_error_t* error;              \
    const auto name = query;                 \
                                             \
    if (error != NULL) {                     \
        Debug::log(ERR, errormsg);           \
        free(error);                         \
        free(name);                          \
        return;                              \
    }                                        \
    free(error);
