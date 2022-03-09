#include "ipc.hpp"
#include "../windowManager.hpp"
#include <string.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

std::string readFromIPCChannel(std::string path) {
    std::ifstream is;
    is.open(path.c_str());

    std::string resultString = std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

    is.close();
    return resultString;
};

int writeToIPCChannel(const std::string path, std::string text) {
    std::ofstream of;
    of.open(path, std::ios::trunc);

    of << text;

    of.close();
    return 0;
}

void IPCSendMessage(const std::string path, SIPCMessageBarToMain smessage) {
    if (!g_pWindowManager->statusBar) {
        Debug::log(ERR, "Tried to write as a bar from the main thread?!");
        return;
    }

    try {
        std::string message = "";

        // write the WID
        message += "wid" + IPC_MESSAGE_EQUALITY + std::to_string(smessage.windowID) + IPC_MESSAGE_SEPARATOR;

        // append the EOF
        message += IPC_END_OF_FILE;

        writeToIPCChannel(path, message);
    } catch (...) {
        Debug::log(WARN, "Error in sending Message B!");
    }
}

void IPCSendMessage(const std::string path, SIPCMessageMainToBar smessage) {
    if (g_pWindowManager->statusBar) {
        Debug::log(ERR, "Tried to write as main from the bar thread?!");
        return;
    }

    try {
        std::string message = "";

        // write active workspace data
        message += "active" + IPC_MESSAGE_EQUALITY + std::to_string(smessage.activeWorkspace) + IPC_MESSAGE_SEPARATOR;

        // write workspace data
        message += "workspaces" + IPC_MESSAGE_EQUALITY;
        for (const auto &w : smessage.openWorkspaces) {
            message += std::to_string(w) + ",";
        }

        message += IPC_MESSAGE_SEPARATOR + "barfullscreenwindow" + IPC_MESSAGE_EQUALITY + (smessage.fullscreenOnBar ? "1" : "0");

        message += IPC_MESSAGE_SEPARATOR + "lastwindowname" + IPC_MESSAGE_EQUALITY + smessage.lastWindowName;

        message += IPC_MESSAGE_SEPARATOR + "lastwindowclass" + IPC_MESSAGE_EQUALITY + smessage.lastWindowClass;

        // append the EOF
        message += IPC_MESSAGE_SEPARATOR + IPC_END_OF_FILE;

        // Send
        writeToIPCChannel(path, message);
    } catch (...) {
        Debug::log(WARN, "Error in sending Message M!");
    }
}

void IPCRecieveMessageB(const std::string path) {
    // recieve message as bar

    if (!g_pWindowManager->statusBar) {
        Debug::log(ERR, "Tried to read as a bar from the main thread?!");
        return;
    }

    try {
        std::string message = readFromIPCChannel(path);

        const auto EOFPOS = message.find(IPC_END_OF_FILE);

        if (EOFPOS == std::string::npos)
            return;

        message = message.substr(0, EOFPOS);

        while (message.find(IPC_MESSAGE_SEPARATOR) != std::string::npos && message.find(IPC_MESSAGE_SEPARATOR) != 0) {
            // read until done.
            const auto PROP = message.substr(0, message.find(IPC_MESSAGE_SEPARATOR));
            message = message.substr(message.find(IPC_MESSAGE_SEPARATOR) + IPC_MESSAGE_SEPARATOR.length());

            // Get the name and value
            const auto PROPNAME = PROP.substr(0, PROP.find(IPC_MESSAGE_EQUALITY));
            const auto PROPVALUE = PROP.substr(PROP.find(IPC_MESSAGE_EQUALITY) + 1);

            if (PROPNAME == "active") {
                try {
                    g_pWindowManager->statusBar->setCurrentWorkspace(stoi(PROPVALUE));
                } catch (...) {
                }  // Try Catch because stoi can be weird
            } else if (PROPNAME == "workspaces") {
                // Read all
                auto propvalue = PROPVALUE;

                g_pWindowManager->statusBar->openWorkspaces.clear();

                while (propvalue.find_first_of(',') != 0 && propvalue.find_first_of(',') != std::string::npos) {
                    const auto WORKSPACE = propvalue.substr(0, propvalue.find_first_of(','));
                    propvalue = propvalue.substr(propvalue.find_first_of(',') + 1);

                    try {
                        g_pWindowManager->statusBar->openWorkspaces.push_back(stoi(WORKSPACE));
                    } catch (...) {
                    }  // Try Catch because stoi can be weird
                }

                // sort
                std::sort(g_pWindowManager->statusBar->openWorkspaces.begin(), g_pWindowManager->statusBar->openWorkspaces.end());
            } else if (PROPNAME == "lastwindowname") {
                g_pWindowManager->statusBar->setLastWindowName(PROPVALUE);
            } else if (PROPNAME == "lastwindowclass") {
                g_pWindowManager->statusBar->setLastWindowClass(PROPVALUE);
            } else if (PROPNAME == "barfullscreenwindow") {
                // deprecated
            }
        }
    } catch(...) {
        Debug::log(WARN, "Error in reading Message B!");
    }
}

void IPCRecieveMessageM(const std::string path) {
    // recieve message as main

    if (g_pWindowManager->statusBar) {
        Debug::log(ERR, "Tried to read as main from the bar thread?!");
        return;
    }

    try {
        std::string message = readFromIPCChannel(path);

        const auto EOFPOS = message.find(IPC_END_OF_FILE);

        if (EOFPOS == std::string::npos)
            return;

        message = message.substr(0, EOFPOS);

        while (message.find(IPC_MESSAGE_SEPARATOR) != std::string::npos && message.find(IPC_MESSAGE_SEPARATOR) != 0) {
            // read until done.
            const auto PROP = message.substr(0, message.find(IPC_MESSAGE_SEPARATOR));
            message = message.substr(message.find(IPC_MESSAGE_SEPARATOR) + IPC_MESSAGE_SEPARATOR.length());

            // Get the name and value
            const auto PROPNAME = PROP.substr(0, PROP.find(IPC_MESSAGE_EQUALITY));
            const auto PROPVALUE = PROP.substr(PROP.find(IPC_MESSAGE_EQUALITY) + 1);

            if (PROPNAME == "wid") {
                try {
                    g_pWindowManager->barWindowID = stoi(PROPVALUE);
                } catch (...) {
                }  // Try Catch because stoi can be weird
            }
        }
    } catch (...) {
        Debug::log(WARN, "Error in reading Message M!");
    }
}