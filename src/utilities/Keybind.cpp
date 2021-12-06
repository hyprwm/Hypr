#include "Keybind.hpp"

Keybind::Keybind(unsigned int mod, xcb_keysym_t keysym, std::string comm, Dispatcher disp) {
    this->m_iMod = mod;
    this->m_Keysym = keysym;
    this->m_szCommand = comm;
    this->m_pDispatcher = disp;
}

Keybind::~Keybind() { }