#include "utilities/Keybind.hpp"
#include <vector>

namespace KeybindManager {
    std::vector<Keybind> keybinds;


    //   -- Methods --    //

    Keybind*            findKeybindByKey(int mod, xcb_keysym_t keysym);
};