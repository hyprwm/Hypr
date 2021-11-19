#pragma once
#include "../defines.hpp"

typedef void (*Dispatcher)(std::string);

enum MODS {
    MOD_NONE = 0,
    MOD_SUPER,
    MOD_SHIFT
};

class Keybind {
public:
    Keybind(MODS, xcb_keysym_t, std::string, Dispatcher);
    ~Keybind();

    EXPOSED_MEMBER(Mod, MODS, i);
    EXPOSED_MEMBER(Keysym, xcb_keysym_t,);
    EXPOSED_MEMBER(Command, std::string, sz);
    EXPOSED_MEMBER(Dispatcher, Dispatcher, p);
};

