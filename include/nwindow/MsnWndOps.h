#pragma once
// The heavier half of the messenger protocol layer's helper types (see MsnProtoOps.h): the functions of the conversation, task and alarm
// windows and of their tables that the sessions' notices reach and that the windows' own files call. The stock keeps them out of line; ours are
// methods of types derived from the window class (adding nothing) so that a hook can take the window as `this`. The bodies are in
// SwitchboardSession.cpp.

#include "nwindow/MsnProtoOps.h"
#include "nwindow/ChatWnd.h"
#include "nwindow/TaskWnd.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/Task.h"
#include "nwindow/TaskBox.h"
#include "nwindow/AlarmWnd.h"
#include "nwindow/MsnTreeCtrl.h"

// A pair of the table of windows by name (MsnWnd.h's NMsnWndStrPair): made in place from the text and the window
struct NMsnWndStrPairInit : public NMsnWndStrPair
{
    __declspec(noinline) NMsnWndStrPairInit* Construct(const TCHAR* InKey, NCWnd* const* InValue);   // 0x101561B0 (ret 8)
};

// The copy of an array of pointers (the stock's 0x10167130 and 0x101466B0 are one code): room for the other's count, then each pointer added at
// the end. `this` is raw memory
struct NMsnPtrArrayCopy : public FArray
{
    __declspec(noinline) NMsnPtrArrayCopy(const TArray<void*>& Src);    // 0x10167130 (ret 4)
};

// 0x10141D50 (ret 4): a copy of a map by name: the pairs copied, no hash yet, the other's hash size, the hash built. `this` is raw memory
struct NMsnNameMapCopier : public NChatNameMap
{
    NChatNameMap* CopyFrom(const NChatNameMap* Src);
};

// The task windows' map (TaskWnd / L2TaskWnd: a conversation window -> its Task)
struct NMsnTaskMapOps : public NTaskMap
{
    NCWnd* FindTask(NCWnd* const* InKey);                                               // 0x101494E0 (ret 4)
    __declspec(noinline) NCWnd** AddTask(NCWnd* const* InKey, NCWnd* const* InValue);   // 0x10149530 (ret 8)
    NCWnd** SetTask(NCWnd* const* InKey, NCWnd* const* InValue);                        // 0x10140D80 (ret 8)
};

// The task list window
struct NMsnTaskWndOps : public TaskWnd
{
    __declspec(noinline) void SetTaskSession(void* InSession, NCWnd* ChatWindow);                      // 0x10140C80 (ret 8)
    __declspec(noinline) Task* FindTaskOfSession(void* InSession);                                    // 0x10141C30 (ret 4)
    __declspec(noinline) void AddTask(SwitchboardSession* InSession, NCWnd* ChatWindow, INT bAlert);   // 0x101435C0 (ret 0xC)
};

// The conversation window: the people of the talk, the session, the notices of people coming and going
struct NMsnChatOps : public ChatWnd
{
    __declspec(noinline) void SetPeerAccount(FString Value);                          // 0x101561F0 (ret 0xC)
    __declspec(noinline) void OnPeerTyping(NChatUser Who);                            // 0x101430C0 (ret 0x44)
    __declspec(noinline) void SetSession(SwitchboardSession* Session);                // 0x101419F0 (ret 4)
    __declspec(noinline) void ShowJoinNotice(NChatUser User);                         // 0x101408F0 (ret 0x44)
    __declspec(noinline) void ShowLeaveNotice(NChatUser User);                        // 0x10140A30 (ret 0x44)
    __declspec(noinline) void OnInvitation(NChatUser From, NMsnMessage Msg);          // 0x10144300 (ret 0x9C)
};

// ChatWnd.cpp: L2TaskWnd: the entry of a friend's conversation (the name by value). Stage 5 (sa-verify): the one definition (it used to be
// declared in MsnUiStockP.h and NConsoleWndExt.h as well)
struct NChatL2TaskWnd : public L2TaskWnd
{
    void AddTask(NCWnd* ChatWindow, FString Name, INT bAlert);   // 0x1014ad00 (ret 0x14)
};
