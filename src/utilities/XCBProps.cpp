#include "XCBProps.hpp"

#include "../windowManager.hpp"

#define DisplayConnection g_pWindowManager->DisplayConnection

std::pair<std::string, std::string> getClassName(int64_t window) {
    PROP(class_cookie, XCB_ATOM_WM_CLASS, 128);

    if (!class_cookiereply) {
        return std::make_pair<>("Error", "Error");
    }

    const size_t PROPLEN = xcb_get_property_value_length(class_cookiereply);
    char* NEWCLASS = (char*)xcb_get_property_value(class_cookiereply);
    const size_t CLASSNAMEINDEX = strnlen(NEWCLASS, PROPLEN) + 1;

    char* CLASSINSTANCE = strndup(NEWCLASS, PROPLEN);
    char* CLASSNAME;
    bool freeClassName = true;
    if (CLASSNAMEINDEX < PROPLEN) {
        CLASSNAME = strndup(NEWCLASS + CLASSNAMEINDEX, PROPLEN - CLASSNAMEINDEX);
    } else {
        CLASSNAME = "";
        freeClassName = false;
    }

    std::string CLASSINST(CLASSINSTANCE);
    std::string CLASSNAM(CLASSNAME);

    free(class_cookiereply);
    free(CLASSINSTANCE);
    if (freeClassName)
        free(CLASSNAME);

    return std::make_pair<>(CLASSINST, CLASSNAM);
}

std::string getRoleName(int64_t window) {
    PROP(role_cookie, HYPRATOMS["WM_WINDOW_ROLE"], 128);

    if (!role_cookiereply)
        return "Error";

    std::string returns = "";

    if (role_cookiereply == NULL || xcb_get_property_value_length(role_cookiereply)) {
        Debug::log(ERR, "Role reply was invalid!");
    } else {
        // get the role

        char* role;
        asprintf(&role, "%.*s", xcb_get_property_value_length(role_cookiereply), (char*)xcb_get_property_value(role_cookiereply));

        returns = role;

        free(role);
    }

    free(role_cookiereply);

    return returns;
}

std::string getWindowName(uint64_t window) {
    PROP(name_cookie, HYPRATOMS["_NET_WM_NAME"], 128);

    if (!name_cookiereply)
        return "Error";

    const int len = xcb_get_property_value_length(name_cookiereply);
    char* name = strndup((const char*)xcb_get_property_value(name_cookiereply), len);
    std::string stringname(name);
    free(name);

    free(name_cookiereply);

    return stringname;
}