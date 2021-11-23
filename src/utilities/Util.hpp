#pragma once

#include "../defines.hpp"
#include <fstream>

std::string exec(const char* cmd);
void clearLogs();
void emptyEvent();

double parabolic(double from, double to, double incline);