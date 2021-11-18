#include "../defines.hpp"

class Keybind {
    EXPOSED_MEMBER(Mod, int, i);
    EXPOSED_MEMBER(Keysym, xcb_keysym_t,);
    EXPOSED_MEMBER(Command, std::string, sz);
    EXPOSED_MEMBER(Dispatcher, void*, p);
};

