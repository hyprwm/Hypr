/*

Hypr Window Manager for X.
Started by Vaxry on 2021 / 11 / 17

*/

#include "windowManager.hpp"

int main(int argc, char** argv) {
    Debug::log(LOG, "Hypr debug log. Built on " + std::string(__DATE__) + " at " + std::string(__TIME__));

    WindowManager::DisplayConnection = xcb_connect(NULL, NULL);
    if (const auto RET = xcb_connection_has_error(WindowManager::DisplayConnection); RET != 0) {
        Debug::log(CRIT, "Connection Failed! Return: " + std::to_string(RET));
        return RET;
    }

    WindowManager::Screen = xcb_setup_roots_iterator(xcb_get_setup(WindowManager::DisplayConnection)).data;

    WindowManager::setupManager();

    Debug::log(LOG, "Hypr Started!");

    while (WindowManager::handleEvent()) {
        ;
    }

    Debug::log(LOG, "Hypr reached the end! Exiting...");

    return 0;
}