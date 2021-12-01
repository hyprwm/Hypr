#pragma once
#include "../defines.hpp"

std::string   readFromIPCChannel(const std::string);
int           writeToIPCChannel(const std::string, std::string);

#define         IPC_END_OF_FILE (std::string)"HYPR_END_OF_FILE"
#define         IPC_MESSAGE_SEPARATOR std::string("\t")
#define         IPC_MESSAGE_EQUALITY std::string("=")

struct SIPCMessageMainToBar {
    std::vector<int>    openWorkspaces;
    uint64_t            activeWorkspace;
    std::string         lastWindowName;
    std::string         lastWindowClass;
    bool                fullscreenOnBar;
};

struct SIPCMessageBarToMain {
    uint64_t            windowID;
};

struct SIPCPipe {
    std::string szPipeName = "";
    uint64_t iPipeFD = 0;
};

// /tmp/ is RAM so the speeds will be decent, if anyone wants to implement
// actual pipes feel free.

void         IPCSendMessage(const std::string, SIPCMessageMainToBar);
void         IPCSendMessage(const std::string, SIPCMessageBarToMain);
void         IPCRecieveMessageB(const std::string);
void         IPCRecieveMessageM(const std::string);