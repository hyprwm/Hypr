#include "XCBProps.hpp"

#include "../windowManager.hpp"

#define DisplayConnection g_pWindowManager->DisplayConnection

std::pair<std::string, std::string> getClassName(int64_t window) {
    PROP(class_cookie, XCB_ATOM_WM_CLASS, 128);

    const size_t PROPLEN = xcb_get_property_value_length(class_cookiereply);
    char* NEWCLASS = (char*)xcb_get_property_value(class_cookiereply);
    const size_t CLASSNAMEINDEX = strnlen(NEWCLASS, PROPLEN) + 1;

    const char* CLASSINSTANCE = strndup(NEWCLASS, PROPLEN);
    const char* CLASSNAME;
    if (CLASSNAMEINDEX < PROPLEN) {
        CLASSNAME = strndup(NEWCLASS + CLASSNAMEINDEX, PROPLEN - CLASSNAMEINDEX);
    } else {
        CLASSNAME = "";
    }

    free(class_cookiereply);

    return std::make_pair<>(std::string(CLASSINSTANCE), std::string(CLASSNAME));
}

std::string getRoleName(int64_t window) {
    PROP(role_cookie, HYPRATOMS["WM_WINDOW_ROLE"], 128);

    std::string returns = "";

    if (role_cookiereply == NULL || xcb_get_property_value_length(role_cookiereply)) {
        Debug::log(ERR, "Role reply was invalid!");
    } else {
        // get the role

        char* role;
        asprintf(&role, "%.*s", xcb_get_property_value_length(role_cookiereply), (char*)xcb_get_property_value(role_cookiereply));

        returns = role;
    }

    free(role_cookiereply);

    return returns;
}