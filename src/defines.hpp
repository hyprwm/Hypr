#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <string>

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

#define STICKS(a, b) abs(a - b) < 2