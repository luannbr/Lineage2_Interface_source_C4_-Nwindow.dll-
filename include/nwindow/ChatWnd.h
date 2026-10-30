#pragma once
// ChatWnd - one conversation of the messenger (UClass "ChatWnd", 0x9F4; C3 only, the messenger that the game's
// NConsoleWnd / Msn code drives). A 256 x 397 window with a title bar (the people's names), the roster of the people in
// it (FriendBox), the text of the talk (ChatBox), an "invite" button (which shows an InviteWnd with the friends that are
// not in yet) and the edit box the player types in. The window talks through a SwitchboardSession (+0xD0) that the Msn
// code gives it; while there is none the messages the player types wait in PendingMessages. Two timers run: 0xA12
// every 3 s (the typing notice for the others) and 0xA14 every 8 s (drops the oldest waiting message).
//
// Layout from the stock constructor 0x1011FD60, OnCreate 0x1011DC20, OnDestroy 0x1011E070, OnTimer, OnPaint and the
// small methods; the records and maps are in NChatTypes.h.

#include "nwindow/NCWnd.h"
#include "nwindow/NChatTypes.h"
#include "nwindow/NChatMsg.h"
#include "nwindow/MsnStockArgs.h"
#pragma pack(push, 4)

class ChatFrameCtrl;
class ChatBox;
class FriendBox;
class InviteWnd;
class NCButton;
class NCEditBox;

class ChatWnd : public NCWnd
{
    NW_DECLARE_CLASS(ChatWnd, NCWnd, 0, 0x1031AFA8)

    void* Messenger;                 // +0xE8 (name ours) cleared by the constructor; the close button tells it (0x101389F0:
                                     //   the window leaves its list)
    void* Msn;                       // +0xEC (name ours) the Msn object (its +0x54 is the player's own NChatUser); also the
                                     //   first constructor argument of the InviteWnd
    void* Switchboard;               // +0xF0 (name ours) the conversation's SwitchboardSession (0 until the server connects)
    TCHAR LastTyped[1040];           // +0xF4 (name ours) the edit box's text at the last typing notice (OnTimer)   // C2: offset derived, no evidence
    INT Unknown8F4;                  // +0x914 (name ours) cleared by the constructor
    INT Unknown8F8;                  // +0x918 1 from the constructor
    INT bHidden;                     // +0x91C (name ours) 1 while the window is hidden or minimized (ShowWindow clears it)
    INT bWarned;                     // +0x920 (name ours) the warning text was shown once (0x1011F920 sets it)
    NChatPtrArray PendingMessages;   // +0x924 (name ours) NChatMessage* (0x58 bytes, destroyed by 0x1012A510) waiting for
                                     //   the switchboard
    NChatPeerMap Peers;              // +0x930 (name ours) the people of the conversation, by timer id
    NChatNameMap Names;              // +0x944 (name ours) the people by account (their NChatUser)
    FString Unknown938;              // +0x958 (name ours)
    NChatUser UserA;                 // +0x964 (name ours)
    NChatUser UserB;                 // +0x9A8 (name ours)
    ChatFrameCtrl* TitleBar;         // +0x9EC (name ours) id 0xA0D, 256 x 20
    ChatBox* TextBox;                // +0x9F0 the talk: 200 lines, 14 shown, 9 / 96 at 238 x 252
    FriendBox* RosterBox;            // +0x9F4 the roster: 300 lines, 2 shown, 9 / 52 at 238 x 30
    NCButton* InviteButton;          // +0x9F8 id 0xA0C, 66 x 21 at (184, 28), SysString 1014
    InviteWnd* InviteWindow;         // +0x9FC 340 x 261 at (-27, 50), hidden until the button shows it
    NCEditBox* EditBox;              // +0xA00 id 0xA0E, 239 x 16 at (8, 360): the text to send
    INT TypingTimer;                 // +0xA04 (name ours) SetTimer(0xA12, 3000)
    INT DropTimer;                   // +0xA08 (name ours) SetTimer(0xA14, 8000)
    INT Unknown9EC;                  // +0xA0C (name ours) 0xA13 from the constructor (a timer id nothing starts here)
    UTexture* BackTex;               // +0xA10 "L2UI_CH3.msn_back3"

    ChatWnd();                                                       // 0x101448f0
    // 0x1011F010 (ret 0x58): the window the messenger makes for a person: (the messenger window, the Msn object, the account the
    // window is about, the player's own record). The account is kept in Unknown938 and the record in UserA; the arguments come by
    // value and are destroyed on return
    ChatWnd(void* InMessenger, void* InMsn, FString InAccount, NChatUser InMe);
    // ~ChatWnd                                                      // 0x10144820

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101450a0
    virtual INT OnTimer(INT TimerID);                                // 36 0x10143ef0
    virtual INT OnCreate();                                          // 37 0x10142790
    virtual INT OnDestroy();                                         // 38 0x10142c00
    virtual void ShowWindow();                                       // 48 0x1013e420 the invite window goes away
    virtual void SetTitle(const TCHAR* Title);                       // 57 0x1013e4a0 the title bar's text
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101413c0

    // names ours
    INT OnMinimize();                                                // 0x1013e450 the minimize button: hidden
    INT OnClose();                                                   // 0x1013e470 the close button: hidden, the messenger told
    INT OnInviteButton();                                            // 0x10144120 the invite button: the invite window with the friends not yet in
    FString GetWarningMsg();                                         // 0x1013e4d0 (hidden return; ret 4) SystemMsg 1313 with $s1 / $s2 put in
    void SetRoster();                                                // 0x10144190 the switchboard's names become the roster and the title
    void RefreshRoster();                                            // 0x10141880 (anonymous in the db) the roster and the title from the names
    UBOOL HasAccount(FString Account);                               // 0x101417b0 (anonymous in the db) (ret 0xC) is that account among the names
    void SendWarning();                                              // 0x101444b0 (anonymous in the db) the warning text sent to the other side and shown
    // 0x1011FEA0 (ret 0x9C, guard: ChatWnd::ReceiveInstantMessage): a message from a person arrives
    void ReceiveInstantMessage(NChatUser From, NMsnMessageArg Msg);   // C4: the message as plain bytes (MsnStockArgs.h), freed here by the stock destructor
    INT OnSend(WPARAM lParam);                                       // 0x10144be0 (ret 4) the edit box's Enter
    // 0x1011E330 (the db called it MsnWnd::PrepareChildWindow): the window is put away for the next conversation: the flags, the
    // waiting messages, the names, the people and the two timers cleared, the switchboard forgotten, the talk, the roster, the
    // invite list and the edit box emptied
    void ResetSession();
};
NW_ASSERT_OFFSET(ChatWnd, Msn, 0xEC);
NW_ASSERT_OFFSET(ChatWnd, Switchboard, 0xF0);
NW_ASSERT_OFFSET(ChatWnd, LastTyped, 0xF4);
NW_ASSERT_OFFSET(ChatWnd, bHidden, 0x91C);
NW_ASSERT_OFFSET(ChatWnd, PendingMessages, 0x924);
NW_ASSERT_OFFSET(ChatWnd, Peers, 0x930);
NW_ASSERT_OFFSET(ChatWnd, Names, 0x944);
NW_ASSERT_OFFSET(ChatWnd, UserA, 0x964);
NW_ASSERT_OFFSET(ChatWnd, UserB, 0x9A8);
NW_ASSERT_OFFSET(ChatWnd, TitleBar, 0x9EC);
NW_ASSERT_OFFSET(ChatWnd, EditBox, 0xA00);
NW_ASSERT_OFFSET(ChatWnd, BackTex, 0xA10);
NW_ASSERT_SIZE(ChatWnd, 0xA14);

#pragma pack(pop)
