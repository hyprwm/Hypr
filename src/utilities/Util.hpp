#pragma once

#include "../defines.hpp"
#include <fstream>
#include <math.h>
#include <array>

// For precise colors
class CFloatingColor {
public:
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 255;

    uint32_t getAsUint32() {
        return ((int)round(a)) * 0x1000000 + ((int)round(r)) * 0x10000 + ((int)round(g)) * 0x100 + ((int)round(b));
    }

    CFloatingColor(uint32_t c) {
        r = RED(c) * 255.f;
        g = GREEN(c) * 255.f;
        b = BLUE(c) * 255.f;
        a = ALPHA(c) * 255.f;
    }

    CFloatingColor() {
        ;
    }

    CFloatingColor& operator=(uint32_t c) {
        r = RED(c) * 255.f;
        g = GREEN(c) * 255.f;
        b = BLUE(c) * 255.f;
        a = ALPHA(c) * 255.f;
        return *this;
    }

    bool operator==(CFloatingColor B) {
        return r == B.r && g == B.g && b == B.b && a == B.a;
    }

    bool operator!=(CFloatingColor B) {
        return !(r == B.r && g == B.g && b == B.b && a == B.a);
    }
};

enum EDockAlign {
    DOCK_LEFT = 0,
    DOCK_RIGHT,
    DOCK_TOP,
    DOCK_BOTTOM
};

std::string exec(const char* cmd);
void clearLogs();
void emptyEvent(xcb_drawable_t window = 0);
void wakeUpEvent(xcb_drawable_t window);
bool xcbContainsAtom(xcb_get_property_reply_t* PROP, xcb_atom_t ATOM);

CFloatingColor parabolicColor(CFloatingColor from, uint32_t to, double incline);
CFloatingColor parabolicColor(CFloatingColor from, CFloatingColor to, double incline);

double parabolic(double from, double to, double incline);

std::vector<std::string> splitString(std::string, char);