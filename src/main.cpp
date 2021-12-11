/*

Hypr Window Manager for X.
Started by Vaxry on 2021 / 11 / 17

*/

#include <fstream>
#include "windowManager.hpp"
#include "defines.hpp"
#include "bar/Bar.hpp"

int main(int argc, char** argv) {
    clearLogs();

    Debug::log(LOG, "Hypr debug log. Built on " + std::string(__DATE__) + " at " + std::string(__TIME__));

    // Create all pipes
    g_pWindowManager->createAndOpenAllPipes();

    Debug::log(LOG, "Pipes done! Forking!");

    if (fork() == 0) {
        // Child. Bar.

        // Sleep for 2 seconds. When launching on a real Xorg session there is some race condition there
        // I don't know where it is but this will fix it for now.
        // Feel free to search for it.
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const int BARRET = barMainThread();
        Debug::log(BARRET == 0 ? LOG : ERR, "Bar exited with code " + std::to_string(BARRET) + "!");
        return 0;
    }

    Debug::log(LOG, "Parent continuing!");

    g_pWindowManager->DisplayConnection = xcb_connect(NULL, NULL);
    if (const auto RET = xcb_connection_has_error(g_pWindowManager->DisplayConnection); RET != 0) {
        Debug::log(CRIT, "Connection Failed! Return: " + std::to_string(RET));
        return RET;
    }

    g_pWindowManager->Screen = xcb_setup_roots_iterator(xcb_get_setup(g_pWindowManager->DisplayConnection)).data;

    if (!g_pWindowManager->Screen) {
        Debug::log(CRIT, "Screen was null!");
        return 1;
    }

    // get atoms
    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(g_pWindowManager->DisplayConnection, 0, ATOM.first.length(), ATOM.first.c_str());
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(g_pWindowManager->DisplayConnection, cookie, NULL);

        if (!reply) {
            Debug::log(ERR, "Atom failed: " + ATOM.first);
            continue;
        }

        ATOM.second = reply->atom;
    }

    g_pWindowManager->setupManager();

    Debug::log(LOG, "Hypr Started!");

    while (g_pWindowManager->handleEvent()) {
        ;
    }

    Debug::log(LOG, "Hypr reached the end! Exiting...");

    xcb_disconnect(g_pWindowManager->DisplayConnection);

    if (const auto err = xcb_connection_has_error(g_pWindowManager->DisplayConnection); err != 0) {
        Debug::log(CRIT, "Exiting because of error " + std::to_string(err));
        return err;
    }

    return 0;
}