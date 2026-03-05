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

void removeAtom(const int& window, xcb_atom_t prop, xcb_atom_t atom) {
    xcb_grab_server(DisplayConnection);

    const auto REPLY = xcb_get_property_reply(DisplayConnection, xcb_get_property(DisplayConnection, false, window, prop, XCB_GET_PROPERTY_TYPE_ANY, 0, 4096), NULL);

    if (!REPLY || xcb_get_property_value_length(REPLY) == 0) {
        free(REPLY);
        xcb_ungrab_server(DisplayConnection);

        return;
    }

    xcb_atom_t* atomsList = (xcb_atom_t*)xcb_get_property_value(REPLY);
    if (!atomsList) {
        free(REPLY);
        xcb_ungrab_server(DisplayConnection);

        return;
    }

    int valuesnum = 0;
    const int current_size = xcb_get_property_value_length(REPLY) / (REPLY->format / 8);
    xcb_atom_t values[current_size];
    for (int i = 0; i < current_size; i++) {
        if (atomsList[i] != atom)
            values[valuesnum++] = atomsList[i];
    }

    xcb_change_property(DisplayConnection, XCB_PROP_MODE_REPLACE, window, prop, XCB_ATOM_ATOM, 32, valuesnum, values);

    free(REPLY);
    xcb_ungrab_server(DisplayConnection);
}

uint8_t getWindowState(const int& win) {
    uint32_t returns = 0;

    const auto COOKIE = xcb_get_property(DisplayConnection, 0, win, HYPRATOMS["_NET_WM_STATE"], HYPRATOMS["_NET_WM_STATE"], 0L, 2L);
    const auto REPLY = xcb_get_property_reply(DisplayConnection, COOKIE, NULL);
    if (REPLY) {
        if (REPLY->type == HYPRATOMS["_NET_WM_STATE"] && REPLY->format == 32 && REPLY->length == 2) {
            returns = *((uint32_t*)xcb_get_property_value(REPLY));
        }
            
        free(REPLY);
    }
    return returns;
}

std::string eventCodeToString(const uint8_t& code) {
    switch (code) {
        case XCB_KEY_PRESS:
            return "XCB_KEY_PRESS";
        case XCB_KEY_RELEASE:
            return "XCB_KEY_RELEASE";
        case XCB_BUTTON_PRESS:
            return "XCB_BUTTON_PRESS";
        case XCB_BUTTON_RELEASE:
            return "XCB_BUTTON_RELEASE";
        case XCB_MOTION_NOTIFY:
            return "XCB_MOTION_NOTIFY";
        case XCB_ENTER_NOTIFY:
            return "XCB_ENTER_NOTIFY";
        case XCB_LEAVE_NOTIFY:
            return "XCB_LEAVE_NOTIFY";
        case XCB_FOCUS_IN:
            return "XCB_FOCUS_IN";
        case XCB_FOCUS_OUT:
            return "XCB_FOCUS_OUT";
        case XCB_KEYMAP_NOTIFY:
            return "XCB_KEYMAP_NOTIFY";
        case XCB_EXPOSE:
            return "XCB_EXPOSE";
        case XCB_GRAPHICS_EXPOSURE:
            return "XCB_GRAPHICS_EXPOSURE";
        case XCB_NO_EXPOSURE:
            return "XCB_NO_EXPOSURE";
        case XCB_VISIBILITY_NOTIFY:
            return "XCB_VISIBILITY_NOTIFY";
        case XCB_CREATE_NOTIFY:
            return "XCB_CREATE_NOTIFY";
        case XCB_DESTROY_NOTIFY:
            return "XCB_DESTROY_NOTIFY";
        case XCB_UNMAP_NOTIFY:
            return "XCB_UNMAP_NOTIFY";
        case XCB_MAP_NOTIFY:
            return "XCB_MAP_NOTIFY";
        case XCB_MAP_REQUEST:
            return "XCB_MAP_REQUEST";
        case XCB_REPARENT_NOTIFY:
            return "XCB_REPARENT_NOTIFY";
        case XCB_CONFIGURE_NOTIFY:
            return "XCB_CONFIGURE_NOTIFY";
        case XCB_CONFIGURE_REQUEST:
            return "XCB_CONFIGURE_REQUEST";
        case XCB_GRAVITY_NOTIFY:
            return "XCB_GRAVITY_NOTIFY";
        case XCB_RESIZE_REQUEST:
            return "XCB_RESIZE_REQUEST";
        case XCB_CIRCULATE_NOTIFY:
            return "XCB_CIRCULATE_NOTIFY";
        case XCB_CIRCULATE_REQUEST:
            return "XCB_CIRCULATE_REQUEST";
        case XCB_PROPERTY_NOTIFY:
            return "XCB_PROPERTY_NOTIFY";
        case XCB_SELECTION_CLEAR:
            return "XCB_SELECTION_CLEAR";
        case XCB_SELECTION_REQUEST:
            return "XCB_SELECTION_REQUEST";
        case XCB_SELECTION_NOTIFY:
            return "XCB_SELECTION_NOTIFY";
        case XCB_COLORMAP_NOTIFY:
            return "XCB_COLORMAP_NOTIFY";
        case XCB_CLIENT_MESSAGE:
            return "XCB_CLIENT_MESSAGE";
        case XCB_MAPPING_NOTIFY:
            return "XCB_MAPPING_NOTIFY";
        case XCB_GE_GENERIC:
            return "XCB_GE_GENERIC";
        default:
            return "UNKNOWN_EVENT";
    }
}
