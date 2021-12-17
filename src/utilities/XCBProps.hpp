#pragma once

#include <iostream>
#include <string.h>
#include <xcb/xcb.h>

std::pair<std::string, std::string> getClassName(int64_t window);
std::string getRoleName(int64_t window);
std::string getWindowName(uint64_t window);

void removeAtom(const int& window, xcb_atom_t prop, xcb_atom_t atom);