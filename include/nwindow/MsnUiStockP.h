#pragma once
// MsnUiStockP.h - stage 5 (the standalone NWindow.dll), lane sa-msn-ui. New file. The part of MsnUiStock.h (read it first) that needs the
// protocol lane's headers (Msn.h, SwitchboardSession.h, FriendList.h, NChatTypes.h...): include it only from a file that includes
// those already (not from the dialogs' files, which use NMsnDlgCommon.h: the two families of headers cannot meet in one file).
//
// The stand-ins whose methods are DEFINED in this lane's ChatWnd.cpp are declared here with their base class (the bodies read its fields);
// the others (defined in SwitchboardSession.cpp, MsnData.cpp...) are declared as the definition declares them, with the same base.

#include "nwindow/MsnUiStock.h"
#include "nwindow/Msn.h"
#include "nwindow/SwitchboardSession.h"
#include "nwindow/FriendList.h"
#include "nwindow/NChatTypes.h"
#include "nwindow/ChatWnd.h"
#include "nwindow/InviteWnd.h"
#include "nwindow/TaskWnd.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/NCListBox.h"
#include "nwindow/MsnProtoOps.h"   // NChatSbSender, NMsnWndOps ... (stage 5, sa-verify: declared there once; this header used to repeat them)
#include "nwindow/MsnWndOps.h"     // NMsnChatOps, NMsnTaskWndOps, NMsnNameMapCopier, NChatL2TaskWnd ...
#pragma pack(push, 4)

class Task;

// ChatWnd.cpp: the protocol object (Msn) as the conversation windows use it
struct NChatMsnRequester : public Msn
{
    void RequestSession(FString Account);                // 0x1014f220 (ret 0xC)
    void CloseSession(SwitchboardSession* Session);      // 0x1014f210 (ret 4)
    NChatUser* GetMe();                                  // 0x1014fc20
};

// ChatWnd.cpp: InviteWnd's list emptied
struct NChatInviteWnd : public InviteWnd
{
    void ClearList();   // 0x1013e330
};

// ChatWnd.cpp: TaskWnd: every task window leaves
struct NChatTaskWnd : public TaskWnd
{
    void DestroyTasks();   // 0x10141c90
};

// MsnSend.cpp (the protocol lane): a command given by value (0x1C bytes, MsnCommand's layout; its struct there is a local one, the same
// name and size). The callee releases it, so the C++ side must not copy or destroy it
struct NMsnCommandArg
{
    BYTE Raw[0x1C];
};
NW_ASSERT_SIZE(NMsnCommandArg, 0x1C);

// MsnSend.cpp: the sender of the sockets (`this` the session): a command followed by a message, both by value (ret 0x74)
struct NMsnSender
{
    void SendMessageBody(NMsnCommandArg Command, NMsnMessageArg Message);       // 0x10160a60
};

// (NMsnWndOps: MsnProtoOps.h; NMsnChatOps, NMsnTaskWndOps, NMsnNameMapCopier: MsnWndOps.h)

// MsnLogin.cpp (the protocol lane): the sign-in of the protocol object, both strings by value (ret 0x18)
struct NMsnManagerLogin : public Msn
{
    void SignIn(NMsnStringArg InAccount, NMsnStringArg InPassword);             // 0x10150120
};

// MsnData.cpp (the protocol lane): the protocol object's constructor and destructor bodies
struct NMsnManagerInit : public Msn
{
    NMsnManagerInit* Construct(MsnWnd* Window);             // 0x10150c40 (ret 4)
    void Destroy();                                         // 0x10150a40
};

#pragma pack(pop)
