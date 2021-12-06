#pragma once
#include "../defines.hpp"

typedef void (*Dispatcher)(std::string);

class Keybind {
public:
    Keybind(unsigned int, xcb_keysym_t, std::string, Dispatcher);
    ~Keybind();

    EXPOSED_MEMBER(Mod, unsigned int, i);
    EXPOSED_MEMBER(Keysym, xcb_keysym_t,);
    EXPOSED_MEMBER(Command, std::string, sz);
    EXPOSED_MEMBER(Dispatcher, Dispatcher, p);
};

