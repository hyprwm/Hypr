#pragma once

#include "../defines.hpp"
#include <fstream>

std::string exec(const char* cmd);
void clearLogs();
void emptyEvent(xcb_drawable_t window = 0);
void wakeUpEvent(xcb_drawable_t window);
bool xcbContainsAtom(xcb_get_property_reply_t* PROP, xcb_atom_t ATOM);

double parabolic(double from, double to, double incline);

std::vector<std::string> splitString(std::string, char);