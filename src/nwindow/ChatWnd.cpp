/*=============================================================================
    ChatWnd.cpp - one conversation of the messenger (C3 only; C3 addresses). Stock: ShowWindow 0x10119b80, the minimize /
    close handlers 0x10119bb0 / 0x10119bd0, SetTitle 0x10119c00, GetWarningMsg 0x10119c30, OnPaint 0x1011c850 ..
    0x1011cbff, OnCreate 0x1011dc20, OnDestroy 0x1011e070, OnTimer 0x1011f360, SetRoster 0x1011f600, the destructor
    0x1011fc90, the constructors 0x1011fd60 / 0x1011f010, ReceiveInstantMessage 0x1011fea0, the Send handler 0x10120050 and
    WindowProc 0x10120510. Wave 5 added ResetSession 0x1011e330, the second constructor and, at the end of the file, the anonymous
    helpers of the conversation windows (session and notification senders, task list entries, getters and thunks, the text
    replacer): see the comment above that block.
=============================================================================*/
#include "nwindow/ChatWnd.h"
#include "nwindow/Msn1Pending.h"
#include "nwindow/ChatBox.h"
#include "nwindow/ChatFrameCtrl.h"
#include "nwindow/FriendBox.h"
#include "nwindow/InviteBox.h"
#include "nwindow/InviteWnd.h"
#include "nwindow/L2Task.h"
#include "nwindow/L2TaskBox.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/Msn.h"
#include "nwindow/MsnUiStockP.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/MsnStockArgs.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NotificationProcess.h"
#include "nwindow/SwitchboardSession.h"
#include "nwindow/MSNSocket.h"
#include "nwindow/TaskBox.h"
#include "nwindow/TaskWnd.h"

enum
{
    NID_CHAT_INVITE = 0xA0C,       // the invite button
    NID_CHAT_FRAME = 0xA0D,        // the title bar
    NID_CHAT_EDIT = 0xA0E,         // the edit box
    TIMER_CHAT_TYPING = 0xA12,     // the typing notice
    TIMER_CHAT_DROP = 0xA14,       // the oldest waiting message goes
    CHAT_UnknownTimerID = 0xA13,
};

// 0x101448f0 - the base window's constructor, the containers empty, the fields cleared (+0x8F8 and +0x8FC are 1)
ChatWnd::ChatWnd()
{
    Messenger = NULL;
    Msn = NULL;
    Switchboard = NULL;
    Unknown8F4 = 0;
    bWarned = 0;
    TitleBar = NULL;
    TextBox = NULL;
    RosterBox = NULL;
    InviteButton = NULL;
    InviteWindow = NULL;
    EditBox = NULL;
    TypingTimer = 0;
    DropTimer = 0;
    Unknown8F8 = 1;
    bHidden = 1;
    Unknown9EC = CHAT_UnknownTimerID;
}
NW_HOOK_CTOR(0x101448f0, ChatWnd)

// 0x10143ba0 (ret 0x58) - the same members as the default constructor, with the Msn object and the messenger from the arguments, the
// account in Unknown938 and the player's record in UserA (0x1012c5b0: every string and the group id copied); +0x8FC and +0x9DC are not
// written (the object comes zeroed from the allocation). The switch of the warning is read from [URL] MSNHideWarnMsg: a failed read
// leaves it 0. The account and the record are the arguments' copies and are destroyed on return
#if NW_HOOK_MODE
__forceinline
#endif
ChatWnd::ChatWnd(void* InMessenger, void* InMsn, FString InAccount, NChatUser InMe)
{
    Msn = InMsn;
    Messenger = InMessenger;
    Unknown938 = InAccount;
    *(NMsnFriend*)&UserA = *(const NMsnFriend*)&InMe;     // NMsnFriend::operator= 0x10151070: the same 0x44-byte record
    Switchboard = NULL;
    Unknown8F4 = 0;
    Unknown8F8 = 1;
    bWarned = 0;
    TitleBar = NULL;
    TextBox = NULL;
    RosterBox = NULL;
    InviteButton = NULL;
    EditBox = NULL;
    TypingTimer = 0;
    DropTimer = 0;
    Unknown9EC = CHAT_UnknownTimerID;
    // sa-emu: the stock calls FConfigCache vslot 0 = GetBool(Section, Key, &Value, NULL) at 0x10143d04 (asm: push 0; push &GMsnHideWarnMsg; push key; push
    // section; call [edx]); this line called GetInt (vslot 1), which reads "True" as 0. SwitchboardSession.cpp makes the same read with GetBool.
    if (!GConfig->GetBool(TEXT("URL"), TEXT("MSNHideWarnMsg"), (UBOOL&)GMsnHideWarnMsg, NULL))
        GMsnHideWarnMsg = 0;
}
NW_HOOK_CTOR_ARGS(0x10143ba0, ChatWnd, (void* A, void* B, FString C, NChatUser D), (A, B, C, D))

// 0x10144820 - the destructor (the two users, the string, the maps and the queue in reverse). The members' destructors are msn2's (the name map's
// 0x10141F90, the message queue's ~NMsnMessage): the hook waits for that lane (Msn1Pending.h; proved in the merged tree)
NW_HOOK_DTOR(0x10144820, ChatWnd)


// 0x101450a0 (vslot 25) - the buttons: invite (0xA0C), minimize (0xF0F0), close (0xF0F1) and the edit box's Enter (0xA0E,
// lParam is passed on)
INT ChatWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CHAT_INVITE)
            return OnInviteButton() != 0;
        if (wParam == NID_FRAME_MINIMIZE)
            return OnMinimize() != 0;
        if (wParam == NID_FRAME_CLOSE)
            return OnClose() != 0;
        if (wParam == NID_CHAT_EDIT)
            return OnSend(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x101450a0, ChatWnd, WindowProc)

// 0x1013e420 (vslot 48) - shown, the invite window hidden (it only comes with the button), no longer minimized
void ChatWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    InviteWindow->HideWindow();
    bHidden = 0;
}
NW_HOOK_METHOD(0x1013e420, ChatWnd, ShowWindow)

// 0x1013e4a0 (vslot 57) - the title bar's text (nothing for NULL)
void ChatWnd::SetTitle(const TCHAR* Title)
{
    if (Title)
        TitleBar->SetTitleText(Title);
}
NW_HOOK_METHOD(0x1013e4a0, ChatWnd, SetTitle)

// 0x1013e450 (name ours) - the minimize button: the window is hidden, and remembered as minimized
INT ChatWnd::OnMinimize()
{
    NCWnd::HideWindow();   // 0x10035ab0, called directly (kept out of line)
    bHidden = 1;
    return 1;
}
NW_HOOK_METHOD(0x1013e450, ChatWnd, OnMinimize)

// 0x1013e470 (name ours) - the close button: hidden, and the messenger told (0x101389f0: the window leaves its list); the
// message is not marked as handled
INT ChatWnd::OnClose()
{
    NCWnd::HideWindow();   // 0x10035ab0
    if (Messenger)
        ((MsnWnd*)Messenger)->OnChatClosed(this);      // MsnWnd::OnChatClosed 0x1015d380
    bHidden = 1;
    return 0;
}
NW_HOOK_METHOD(0x1013e470, ChatWnd, OnClose)

// the global the messenger's buttons read before they act (0x10322C68; set by MsnCheckVersion, MessengerWnd.cpp)
#if NW_HOOK_MODE
#define GMsnBlocked NW_GLOBAL(INT, 0x10322C68)
#else
extern INT GMsnBlocked;                 // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

// 0x10144120 (name ours) - the invite button: while the messenger is not busy (the global at 0x102c3200 is 0) the invite window
// gets the friends that are not in the conversation (InviteWnd 0x1011ede0), is shown and takes the focus; otherwise the
// messenger (when there is one) shows SystemMsg 1309 (0x101337c0)
INT ChatWnd::OnInviteButton()
{
    if (GMsnBlocked == 0)
    {
        InviteWindow->FillFriends();                   // InviteWnd::FillFriends 0x10143960
        InviteWindow->ShowWindow();
        InviteWindow->SetFocus();
    }
    else if (Messenger)
    {
        ((MsnWnd*)Messenger)->ShowMessageDialog(*GL2GameData.SystemMsg[1309].Message);   // MsnWnd::ShowMessageDialog 0x10158460
        return 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x10144120, ChatWnd, OnInviteButton)

// 0x1013e4d0 (ret 4, guard-less) - SystemMsg 1313 with "$s1" replaced by the second user's decoded name (if the text has no
// "$s1", the first user's name goes in front of it) and "$s2" by the first user's
FString ChatWnd::GetWarningMsg()
{
    FString Template(*GL2GameData.SystemMsg[1313].Message);
    FString NameA = UserA.GetDecodedName();
    FString NameB = UserB.GetDecodedName();
    FString Text = Template.Substitute(FString(TEXT("$s1")), NameB);
    if (Text == Template)
        Text = NameA + Template;
    Text = Text.Substitute(FString(TEXT("$s2")), NameA);
    return Text;
}
NW_HOOK_METHOD(0x1013e4d0, ChatWnd, GetWarningMsg)

// 0x101417b0 (ret 0xC) - is a person of the names (by account, compared without case) the one given; the argument is destroyed
UBOOL ChatWnd::HasAccount(FString Account)
{
    NChatNameIter It(Names);
    for (It.Index = 0; It.Index < Names.Pairs.Num(); It.Index++)
    {
        NChatUser* User = Names.Pairs(It.Index).Value;
        if (User && User->Account == Account)
            return 1;
    }
    return 0;
}
typedef UBOOL (ChatWnd::*SigHasAccount)(FString);
NW_HOOK_VARARGS(0x101417b0, ChatWnd, HasAccount, SigHasAccount)

// 0x10144190 - with a switchboard: its names (the array at +0x80, the table size at +0x90) replace ours, the roster is emptied
// and filled with a copy of each person (in the usual color), the title is the session's title and the "several people" flag
// is set when there are more than one
void ChatWnd::SetRoster()
{
    if (Switchboard)
    {
        BYTE* Names80 = (BYTE*)Switchboard + 0x80;
#if NW_HOOK_MODE
        ::nw::StockCallAt<void>(0x10141120, &Names.Pairs, Names80);   // TArray<NChatNamePair>::operator=
#else
        Names.Pairs = *(const NChatNameArray*)Names80;                // the same assignment (the standalone compiles the template)
#endif
        Names.HashCount = *(INT*)(Names80 + 0x10);
        Names.Rehash();
        RosterBox->Clear();
        NChatNameIter It(Names);
        for (It.Index = 0; It.Index < Names.Pairs.Num(); It.Index++)
        {
            NChatUser* User = Names.Pairs(It.Index).Value;
            if (User)
                RosterBox->AddUser(*User, 0xFFDCDCDC);
        }
        FString Title = ((SwitchboardSession*)Switchboard)->GetSessionTitle();   // SwitchboardSession::GetSessionTitle 0x101535F0
        SetTitle(*Title);
        Unknown8F4 = Names.Pairs.Num() > 1;
    }
}
NW_HOOK_METHOD(0x10144190, ChatWnd, SetRoster)

// 0x10141880 (name ours) - the roster emptied and filled from the names (as SetRoster) while a title is built from the decoded
// names separated by commas; the title (less its last character) is set and the flag as in SetRoster
void ChatWnd::RefreshRoster()
{
    RosterBox->Clear();
    FString Title;
    NChatNameIter It(Names);
    for (It.Index = 0; It.Index < Names.Pairs.Num(); It.Index++)
    {
        NChatUser* User = Names.Pairs(It.Index).Value;
        if (User)
        {
            RosterBox->AddUser(*User, 0xFFDCDCDC);
            Title += User->GetDecodedName();
            Title += TEXT(",");
        }
    }
    Title = Title.Left((Title.Num() ? Title.Num() - 1 : 0) - 1);
    SetTitle(*Title);
    Unknown8F4 = Names.Pairs.Num() > 1;
}
NW_HOOK_METHOD(0x10141880, ChatWnd, RefreshRoster)

// the messenger's notice of a message to the console (0x10061190: a jump to the console's virtual 264; ret 0xC)
static FORCEINLINE void NMsnNotify(const TCHAR* Account, const TCHAR* Text, INT Flag)
{
    GConsole->RequestSendMsnChatLog((TCHAR*)Account, (TCHAR*)Text, Flag);        // NConsoleWnd::RequestSendMsnChatLog 0x1006CA90
}

// 0x10144be0 (name ours, ret 4) - the edit box's Enter (lParam 1) with a text, unless the player's own status is hidden
// ("HDN"): the text becomes a message in Arial. With a switchboard the warning is sent once (unless hidden), the message goes
// out (0x1012e4d0), is shown under the player's own name and each person of the names is notified. Without one it is shown
// too, and if the other party (their account is UserB's) is a friend who is not offline, the warning (once) and the message
// are queued as heap records and the messenger is asked for a session (0x1012a750); otherwise a notice (SystemMsg 1327, the
// offline icon) and the text indented are shown. The edit box is emptied
INT ChatWnd::OnSend(WPARAM lParam)
{
    if (lParam == 1 && EditBox->CommitText())
    {
        FString MyStatus = ((::Msn*)Msn)->GetStatus();                          // Msn::GetStatus 0x1014F1D0
        UBOOL bHideSend = MyStatus == TEXT("HDN");
        if (bHideSend)
            return 1;

        NMsnMessageLocal Msg;
        Msg->SetBody(FString(EditBox->CommitText()));
        Msg->SetFontName(FString(TEXT("Arial")));
        Msg->Color = 0;
        Msg->SetEffects(FString(TEXT("l")));
        if (Switchboard)
        {
            if (bWarned == 0 && GMsnHideWarnMsg == 0)
                SendWarning();
            ((NChatSbSender*)Switchboard)->SendText(MsnMessageArgOf(Msg.Ref()));      // 0x101530D0
            TextBox->AddMessage(EditBox->CommitText(), ((NChatMsnRequester*)Msn)->GetMe(), 0xFFDCDCDC);   // 0x1014FC20
            NChatNameIter It(Names);
            for (It.Index = 0; It.Index < Names.Pairs.Num(); It.Index++)
            {
                NChatUser* User = Names.Pairs(It.Index).Value;
                if (User)
                    NMsnNotify(*User->Account, EditBox->CommitText(), 0);
            }
        }
        else
        {
            TextBox->AddMessage(EditBox->CommitText(), ((NChatMsnRequester*)Msn)->GetMe(), 0xFFDCDCDC);
            NChatUser* Friend = (NChatUser*)((::Msn*)Msn)->FindFriend(UserB.Account);   // Msn::FindFriend 0x1014FC30
            if (Friend && Friend->Status != TEXT("FLN"))
            {
                if (bWarned == 0 && GMsnHideWarnMsg == 0)
                {
                    NMsnMessage* Warning = new NMsnMessage;
                    TCHAR Text[2048];
                    DWORD* Fill = (DWORD*)Text;
                    for (INT i = 0x400; i != 0; i--)
                        *Fill++ = 0;
                    {
                        FString WarningText = GetWarningMsg();
                        appSprintf(Text, TEXT("%s"), *WarningText);
                    }
                    Warning->SetBody(FString(Text));
                    Warning->SetFontName(FString(TEXT("Arial")));
                    Warning->Color = 8000;
                    Warning->SetEffects(FString(TEXT("l")));
                    void* Item = Warning;
#if NW_HOOK_MODE
                    ::nw::StockCallAt<INT>(0x100A5A00, &PendingMessages, &Item);   // TPtrArray::AddItem
#else
                    PendingMessages.AddItem(Item);                                 // the standalone compiles the template
#endif
                    bWarned = 1;
                }
                void* Queued = new NMsnMessage(Msg.Ref());
#if NW_HOOK_MODE
                ::nw::StockCallAt<INT>(0x100A5A00, &PendingMessages, &Queued);
#else
                PendingMessages.AddItem(Queued);
#endif
                if (Msn)
                    ((NChatMsnRequester*)Msn)->RequestSession(UserB.Account);       // 0x1014F220
            }
            else
            {
                TCHAR Line[2048];
                TextBox->AddWithIcon((INT)FindTexture(TEXT("L2UI_CH3.Msn.msnicon7"), 1), *GL2GameData.SystemMsg[1327].Message, 0xFFE37171);
                appSprintf(Line, TEXT("   %s"), EditBox->CommitText());
                TextBox->AddMessage(Line, NULL, 0xFFE37171);
            }
        }
        EditBox->ClearText();
    }
    return 1;
}
NW_HOOK_METHOD(0x10144be0, ChatWnd, OnSend)

// 0x101444b0 (name ours) - the warning (GetWarningMsg) as a text message in Arial (color 8000), sent through the switchboard
// when there is one (0x1012e4d0, the message by value) and shown in the talk (the player's own user, color 0xFF8CB48C); with a
// switchboard the warning is marked as given, and each person of the names is notified with the edit box's text
void ChatWnd::SendWarning()
{
    NMsnMessageLocal Msg;
    TCHAR Text[2048];
    DWORD* Fill = (DWORD*)Text;   // cleared with `rep stosd` in the stock code (0x400 dwords)
    for (INT i = 0x400; i != 0; i--)
        *Fill++ = 0;
    {
        FString Warning = GetWarningMsg();
        appSprintf(Text, TEXT("%s"), *Warning);
    }
    Msg->Body = FString(Text);
    Msg->FontName = FString(TEXT("Arial"));
    Msg->Color = 8000;
    Msg->Effects = FString(TEXT("l"));
    if (Switchboard)
    {
        ((NChatSbSender*)Switchboard)->SendText(MsnMessageArgOf(Msg.Ref()));          // 0x101530D0
        TextBox->AddMessage(Text, ((NChatMsnRequester*)Msn)->GetMe(), 0xFF8CB48C);       // 0x1014FC20
        bWarned = 1;
        NChatNameIter It(Names);
        for (It.Index = 0; It.Index < Names.Pairs.Num(); It.Index++)
        {
            NChatUser* User = Names.Pairs(It.Index).Value;
            if (User)
                NMsnNotify(*User->Account, EditBox->CommitText(), 0);
        }
    }
}
NW_HOOK_METHOD(0x101444b0, ChatWnd, SendWarning)

// 0x10144a30 (ret 0x9C, guard: ChatWnd::ReceiveInstantMessage) - the alarm sound; the typing timers of the sender's entries
// (matched by account) are killed and the entries removed; the text goes into the talk under the sender's name; the warning is
// sent once (unless hidden); the console is notified (account, text, 1). Both arguments come by value and are destroyed
void ChatWnd::ReceiveInstantMessage(NChatUser From, NMsnMessageArg Msg)
{
    ((::Msn*)Msn)->PlayAlarmSound(1);   // Msn::PlayAlarmSound 0x1014FCA0
    NChatPeerIterLocal It(Peers);
    for (It.Index = 0; It.Index < Peers.Pairs.Num(); It.Index++)
    {
        UBOOL bSame = Peers.Pairs(It.Index).Value.GetUser().Account == From.Account;
        if (bSame)
        {
            KillTimer(Peers.Pairs(It.Index).Value.TimerID);
            Peers.RemoveKey(Peers.Pairs(It.Index).Key);
        }
    }
    TextBox->AddMessage(*Msg.Msg().Body, &From, 0xFFDCDCDC);
    if (bWarned == 0 && GMsnHideWarnMsg == 0)
        SendWarning();
    NMsnNotify(*From.Account, *Msg.Msg().Body, 1);
    MsnMessageFree(Msg.Raw);          // the message is the callee's: freed here, as the stock does
}
typedef void (ChatWnd::*SigReceiveInstantMessage)(NChatUser, NMsnMessageArg);
// installed as the member itself (no forwarder): the by-value arguments a forwarder would copy again are the stock's own
NW_HOOK_VARARGS(0x10144a30, ChatWnd, ReceiveInstantMessage, SigReceiveInstantMessage)

// 0x10143ef0 (vslot 36) - the typing timer (0xA12): when the edit box has a text that differs from the last one noticed and
// there is a switchboard, a typing notice (a message with the player's own account, 0x1012e3e0, by value) goes out and the
// text is remembered; whatever the timer, the entries of the people with that key lose their timers and go; the drop timer
// (0xA14) without a switchboard drops the oldest waiting message
INT ChatWnd::OnTimer(INT TimerID)
{
    if (TimerID == TIMER_CHAT_TYPING && EditBox->CommitText())
    {
        if (appStrcmp(LastTyped, EditBox->CommitText()) != 0 && Switchboard)
        {
            NMsnMessageLocal Msg;
            NChatUser* Me = ((NChatMsnRequester*)Msn)->GetMe();                  // 0x1014FC20
            Msg->SetBody(Me->Account);
            ((NChatSbSender*)Switchboard)->SendTyping(MsnMessageArgOf(Msg.Ref()));        // 0x10152FE0
            appStrcpy(LastTyped, EditBox->CommitText());
        }
    }
    NChatPeerIterLocal It(Peers);
    for (It.Index = 0; It.Index < Peers.Pairs.Num(); It.Index++)
    {
        if (TimerID == Peers.Pairs(It.Index).Key)
        {
            KillTimer(Peers.Pairs(It.Index).Value.TimerID);
            Peers.RemoveKey(Peers.Pairs(It.Index).Key);
        }
    }
    if (TimerID == TIMER_CHAT_DROP && Switchboard == NULL && PendingMessages.Num() > 0)
    {
        void* First = PendingMessages(0);
        if (First)
        {
            ((NMsnMessage*)First)->~NMsnMessage();   // the message record's destructor 0x1014EFE0
            appFree(First);
        }
        PendingMessages.Remove(0, 1);
    }
    return 1;
}
NW_HOOK_METHOD(0x10143ef0, ChatWnd, OnTimer)

// 0x10142790 (vslot 37) - the back texture, the title bar (id 0xA0D, as wide as the window), the roster (300 lines, 2 shown),
// the talk (200 lines, 14 shown), the invite button, the invite window (hidden until the button shows it), the edit box and
// the two timers
INT ChatWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.msn_back3"));

    TitleBar = new ChatFrameCtrl(1, 0, 0);
    FrameID = NID_CHAT_FRAME;
    TitleBar->ID = NID_CHAT_FRAME;
    TitleBar->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(TitleBar, NWS_VISIBLE);

    RosterBox = new FriendBox(300, 2, 0xF);
    RosterBox->SetPosition(9, 0x34, 0xEE, 0x1E);
    CreateChild(RosterBox, NWS_VISIBLE);

    TextBox = new ChatBox(200, 0xE, 0xF);
    TextBox->SetPosition(9, 0x60, 0xEE, 0xFC);
    CreateChild(TextBox, NWS_VISIBLE);

    InviteButton = new NCButton(TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), 0);
    InviteButton->ID = NID_CHAT_INVITE;
    InviteButton->SetPosition(0xB8, 0x1C, 0x42, 0x15);
    CreateChild(InviteButton, NWS_VISIBLE);
    InviteButton->SetTextID(1014);

    InviteWindow = new InviteWnd(Msn, Switchboard);
    InviteWindow->SetPosition(-0x1B, 0x32, 0x154, 0x105);
    CreateChild(InviteWindow, NWS_VISIBLE | NWS_NOCLOSE);
    ::nw::VirtualCall<void>(InviteWindow, 56, 1014);                 // SetTitleID(1014): the C4 stock calls the slot 56 with the SysString id

    EditBox = new NCEditBox(0, 0, 1, 0x12C0, 0);
    EditBox->ID = NID_CHAT_EDIT;
    EditBox->SetPosition(8, 0x168, 0xEF, 0x10);
    CreateChild(EditBox, NWS_VISIBLE);

    TypingTimer = SetTimer(TIMER_CHAT_TYPING, 3000, 0);
    DropTimer = SetTimer(TIMER_CHAT_DROP, 8000, 0);
    return 1;
}
NW_HOOK_METHOD(0x10142790, ChatWnd, OnCreate)

// 0x10142c00 (vslot 38) - the waiting messages are destroyed and freed; the names are forgotten; the typing timers of the
// people are killed and the people forgotten; the window's own timers go; the six children go; then the base
INT ChatWnd::OnDestroy()
{
    for (INT i = 0; i < PendingMessages.Num(); i++)
    {
        void* Message = PendingMessages(i);
        if (Message)
        {
            ((NMsnMessage*)Message)->~NMsnMessage();   // the message record's destructor 0x1014EFE0
            appFree(Message);
        }
    }
    PendingMessages.Empty();

    for (INT i = 0; i < Names.Pairs.Num(); i++)
        Names.Pairs(i).Key.~FString();
    Names.Pairs.FArray::Empty(sizeof(NChatNamePair));
    Names.HashCount = 8;
    Names.Rehash();

    for (INT i = 0; i < Peers.Pairs.Num(); i++)
        KillTimer(Peers.Pairs(i).Value.TimerID);
    for (INT i = 0; i < Peers.Pairs.Num(); i++)
        Peers.Pairs(i).Value.User.~NChatUser();
    Peers.Pairs.FArray::Empty(sizeof(NChatPeerPair));
    Peers.HashCount = 8;
    Peers.Rehash();

    if (TypingTimer)
    {
        KillTimer(TypingTimer);
        TypingTimer = 0;
    }
    if (DropTimer)
    {
        KillTimer(DropTimer);
        DropTimer = 0;
    }
    Unknown9EC = 0;
    Switchboard = NULL;

    NDestroyChild(this, TitleBar);
    NDestroyChild(this, TextBox);
    NDestroyChild(this, RosterBox);
    NDestroyChild(this, InviteButton);
    NDestroyChild(this, InviteWindow);
    NDestroyChild(this, EditBox);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10142c00, ChatWnd, OnDestroy)

// 0x10142ec0 (name ours; the db called it MsnWnd::PrepareChildWindow) - the window put away to be used for another conversation: the
// flags cleared, the waiting messages destroyed and freed, the names and the people forgotten (their typing timers killed), the two
// timers killed, the timer id back to its start value, the switchboard forgotten; the talk, the roster and the invite list are
// emptied (virtual 71) and the edit box too (virtual 70). The window itself and its children stay
void ChatWnd::ResetSession()
{
    Unknown8F4 = 0;
    Unknown8F8 = 0;
    bWarned = 0;
    for (INT i = 0; i < PendingMessages.Num(); i++)
    {
        void* Message = PendingMessages(i);
        if (Message)
        {
            ((NMsnMessage*)Message)->~NMsnMessage();   // the message record's destructor 0x1014EFE0
            appFree(Message);
        }
    }
    PendingMessages.Empty();

    for (INT i = 0; i < Names.Pairs.Num(); i++)
        Names.Pairs(i).Key.~FString();
    Names.Pairs.FArray::Empty(sizeof(NChatNamePair));
    Names.HashCount = 8;
    Names.Rehash();

    for (INT i = 0; i < Peers.Pairs.Num(); i++)
        KillTimer(Peers.Pairs(i).Value.TimerID);
    for (INT i = 0; i < Peers.Pairs.Num(); i++)
        Peers.Pairs(i).Value.User.~NChatUser();
    Peers.Pairs.FArray::Empty(sizeof(NChatPeerPair));
    Peers.HashCount = 8;
    Peers.Rehash();

    if (TypingTimer)
    {
        KillTimer(TypingTimer);
        TypingTimer = 0;
    }
    if (DropTimer)
    {
        KillTimer(DropTimer);
        DropTimer = 0;
    }
    Unknown9EC = CHAT_UnknownTimerID;
    Switchboard = NULL;
    TextBox->Clear();
    RosterBox->Clear();
    ((NChatInviteWnd*)InviteWindow)->ClearList();   // InviteWnd's list emptied 0x1013E330
    EditBox->ClearText();
}
NW_HOOK_METHOD(0x10142ec0, ChatWnd, ResetSession)

// 0x101413c0 (vslot 56, guard: ChatWnd::OnPaint) - the back; while a switchboard is up, "<SysString 1012>" at (8, 34) and
// "<number of names><SysString 1013>" after it; at the bottom (9, 381) the names of the people (one: the name, a space and
// SystemMsg 1346; more: SysString 1025, ':' and the names separated by ',') cut to 238 pixels
INT ChatWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Cut[260];
    TCHAR Line[260];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0x14, 0x100, 0x17D, 0, 0, 0x100, 0x17D, BackTex, 0xFF, 1);
    if (Switchboard)
    {
        appStrcpy(Line, *GL2GameData.SysString[1012]);
        Device->DrawText(8, 0x22, 0xFFDCDCDC, Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, Line, 0, 0, 0, 0, 0);
        appSprintf(Line, TEXT("%d%s"), Names.Pairs.Num(), *GL2GameData.SysString[1013]);
        Device->DrawText(Size.cx + 10, 0x22, 0xFFDCDCDC, Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    FString Title;
    if (Peers.Pairs.Num() > 1)
    {
        Title = *GL2GameData.SysString[1025];
        Title += TEXT(":");
        for (INT i = 0; i < Peers.Pairs.Num(); i++)
        {
            NChatUser Person = Peers.Pairs(i).Value.GetUser();
            Title += Person.GetDecodedName();
            Title += TEXT(",");
        }
        Title = Title.Left((Title.Num() ? Title.Num() - 1 : 0) - 1);
    }
    else if (Peers.Pairs.Num() == 1)
    {
        NChatUser Person = Peers.Pairs(0).Value.GetUser();
        Title += Person.GetDecodedName();
        Title += TEXT(" ");
        Title += *GL2GameData.SystemMsg[1346].Message;
    }
    NFitText(*Title, Cut, 0xEE, 0xC);
    Device->DrawText(9, 0x17D, 0xFFDCDCDC, Cut, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x101413c0, ChatWnd, OnPaint)

/*=============================================================================
    The helpers of the conversation windows that the db lists as anonymous functions: the senders the window calls on the
    session and on the notification connection, the small getters and thunks the window code reaches, the task list's entry
    points, the text replacer and the map lookups. Each belongs to a class that is another lane's file, so the hooks live here
    on small derived structs (never constructed: they only carry the method) or on free functions with the stock calling
    convention (regcheck wants a method for every function that reads ecx at entry).
=============================================================================*/

/*-----------------------------------------------------------------------------
    SwitchboardSession: the three senders the window uses (the session's own are SwitchboardSession.cpp's). NChatSbSender is declared in
    MsnUiStockP.h.
-----------------------------------------------------------------------------*/

// The session's body sender (the protocol lane's NMsnSender::SendMessageBody, stock 0x10160A60, ret 0x74): the command (by value, a copy the
// callee frees) and the message (a copy made by the stock copy constructor, which the callee frees). Both are plain bytes in the argument
// slots: the callee owns what they hold
static FORCEINLINE void SbSendMessageBody(void* Session, MsnCommand& Command, NMsnMessageArg& Message)
{
    // the real sender (MsnSend.cpp) in both builds: sa-msn-proto removed NMsnSender (MSNSocket::SendMessageBody is the function now), and the
    // hook build called the removed declaration (it stopped linking after the merge: unresolved NMsnSender::SendMessageBody). In the hook
    // build the stock entry 0x10160A60 jumps to this same function anyway
    ((MSNSocket*)Session)->SendMessageBody(NMsnLocalCommand(Command), MsnMessageArgOf(Message.Msg()));
}

// 0x10152fe0 (ret 0x58) - a typing notice: the message is made a typing one (Kind 1), "MSG trid U" goes with it; the message given is freed.
// The stock builds the argument "U" with the narrow FString constructor; ours with the wide literal, the same text
void NChatSbSender::SendTyping(NMsnMessageArg Message)
{
    Message.Msg().Kind = NMSN_Typing;
    NMsnLocalCommand Command(FString(TEXT("MSG")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("U")));
    SbSendMessageBody(this, Command, Message);
    MsnMessageFree(Message.Raw);
}
typedef void (NChatSbSender::*SigChatSendMessage)(NMsnMessageArg);
NW_HOOK_VARARGS(0x10152fe0, NChatSbSender, SendTyping, SigChatSendMessage)

// 0x101530d0 (ret 0x58) - a text: the message is made Kind 2, "MSG trid N" goes with it; the message given is freed. The stock builds the
// argument "N" with the narrow FString constructor; ours with the wide literal, the same text
void NChatSbSender::SendText(NMsnMessageArg Message)
{
    Message.Msg().Kind = NMSN_Unknown2;
    NMsnLocalCommand Command(FString(TEXT("MSG")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("N")));
    SbSendMessageBody(this, Command, Message);
    MsnMessageFree(Message.Raw);
}
NW_HOOK_VARARGS(0x101530d0, NChatSbSender, SendText, SigChatSendMessage)

// 0x101532b0 (ret 0xC) - "CAL trid account": that account is invited into the conversation. The stock builds the verb with the narrow FString
// constructor; ours with the wide literal, the same text
void NChatSbSender::SendInvite(FString Account)
{
    NMsnLocalCommand Command(FString(TEXT("CAL")));
    Command.TrID = NextTrID;
    Command.AddArg(Account);
    Send(Command);
}
NW_HOOK_VARARGS(0x101532b0, NChatSbSender, SendInvite, void (NChatSbSender::*)(FString))

/*-----------------------------------------------------------------------------
    The notification connection: the request for a switchboard (XFR SB) and the map that remembers whom it was for.
-----------------------------------------------------------------------------*/

struct NChatIntStrPair : public NMsnIntStrPair
{
    NChatIntStrPair* Init(const INT* InKey, const TCHAR* InValue);   // 0x1013C510 (ret 8)
};

// 0x10160f30 (ret 8, name ours) - a pair of the transaction map made in place: the number and a copy of the account (the chain link is
// set by the map)
__declspec(noinline) NChatIntStrPair* NChatIntStrPair::Init(const INT* InKey, const TCHAR* InValue)
{
    Key = *InKey;
    new ((void*)&Value) FString(InValue);
    return this;
}
typedef NChatIntStrPair* (NChatIntStrPair::*SigChatPairInit)(const INT*, const TCHAR*);
NW_HOOK_VARARGS(0x10160f30, NChatIntStrPair, Init, SigChatPairInit)

struct NChatIntStrMap : public NMsnIntStrMap
{
    FString* Set(const INT& InKey, const TCHAR* Value);   // 0x1013E850 (ret 8, name ours)
};

// 0x10163270 (ret 8, name ours) - UE2's TMapBase::Set: the string of the pair with that key is replaced, or a pair is added (0x10131580:
// `new(Pairs) T`, the number and the string made by 0x1013c510), chained in the hash, and the table doubled when the pairs outgrow it
FString* NChatIntStrMap::Set(const INT& InKey, const TCHAR* Value)
{
    for (INT i = Hash[InKey & (HashCount - 1)]; i != -1; i = Pairs(i).HashNext)
    {
        if (Pairs(i).Key == InKey)
        {
            Pairs(i).Value = Value;
            return &Pairs(i).Value;
        }
    }
#if NW_HOOK_MODE
    NChatIntStrPair* Pair = reinterpret_cast<NChatIntStrPair*>(((void*(__cdecl*)(INT, void*))::nw::Addr(0x101561d0))(0x14, this));   // new(Pairs) T: the stock's shared copy
#else
    NChatIntStrPair* Pair = reinterpret_cast<NChatIntStrPair*>(&Pairs(Pairs.Add(1)));   // new(Pairs) T: one more element (the standalone compiles the template)
#endif
    if (Pair)
        Pair = Pair->Init(&InKey, Value);
    INT iHash = Pair->Key & (HashCount - 1);
    Pair->HashNext = Hash[iHash];
    Hash[iHash] = Pairs.Num() - 1;
    if (HashCount * 2 + 8 < Pairs.Num())
    {
        HashCount = HashCount * 2;
        Rehash();
    }
    return &Pair->Value;
}
NW_HOOK_METHOD(0x10163270, NChatIntStrMap, Set)

struct NChatNotifySender : public NotificationProcess
{
    __declspec(noinline) void RequestSwitchboard(FString Account);   // 0x10164910 (ret 0xC)
};

// 0x10164910 (ret 0xC) - "XFR trid SB" (request type 9) is sent, and the account it is for is kept under the transaction id, so that the
// answer (the switchboard's address) finds whom the session was asked for. The stock builds the verb and the argument with the narrow
// FString constructor (from "XFR" and "SB"); ours with the wide literal, the same text
void NChatNotifySender::RequestSwitchboard(FString Account)
{
    NMsnLocalCommand Command(FString(TEXT("XFR")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("SB")));
    RegisterPending(9);
    static_cast<NChatIntStrMap&>(Names).Set(NextTrID, *Account);
    Send(Command);
}
NW_HOOK_VARARGS(0x10164910, NChatNotifySender, RequestSwitchboard, void (NChatNotifySender::*)(FString))

// (NChatMsnRequester is declared in MsnUiStockP.h)

// 0x1014f220 (ret 0xC) - a conversation with that account is wanted: the notification connection (when there is one) is asked for a switchboard
void NChatMsnRequester::RequestSession(FString Account)
{
    if (Notification)
        static_cast<NChatNotifySender*>(Notification)->RequestSwitchboard(Account);
}
NW_HOOK_VARARGS(0x1014f220, NChatMsnRequester, RequestSession, void (NChatMsnRequester::*)(FString))

// 0x1014f210 (ret 4, name ours; the manager is `this` for the callers but the code never reads it) - a session to leave: "OUT" for the
// one given, if any
void NChatMsnRequester::CloseSession(SwitchboardSession* Session)
{
    if (Session)
        Session->Close();
}
NW_HOOK_METHOD(0x1014f210, NChatMsnRequester, CloseSession)

/*-----------------------------------------------------------------------------
    Small thunks and getters (several are folded by the linker with other classes' copies: the code is the same for all).
-----------------------------------------------------------------------------*/

// 0x1014fc20 (name ours) - the manager's own record (Msn +0x54)
NChatUser* NChatMsnRequester::GetMe()
{
    return reinterpret_cast<NChatUser*>(Me);
}
NW_HOOK_METHOD(0x1014fc20, NChatMsnRequester, GetMe)

#if NW_HOOK_MODE
// (the stock stubs of the console's two senders: nothing calls them; the real methods are NConsoleWnd::RequestSendL2FriendSay /
// RequestSendMsnChatLog in NConsoleWndC4.cpp, which the standalone calls)
struct NChatConsole : public UL2ConsoleWnd
{
    void SendL2FriendSay(TCHAR* Name, TCHAR* Text);              // 0x10061180 (ret 8)
    void SendMsnChatLog(TCHAR* Account, TCHAR* Text, INT Flag);  // 0x10061190 (ret 0xC)
};

// 0x1006ca80  NOT PORTED (eligible.csv: insn 0 this+48 -> 54 (layout says 48))
NW_NAKED void NChatConsole::SendL2FriendSay(TCHAR* Name, TCHAR* Text) NW_STOCK(0x1006ca80)

// 0x1006ca90  NOT PORTED (eligible.csv: insn 0 this+48 -> 54 (layout says 48))
NW_NAKED void NChatConsole::SendMsnChatLog(TCHAR* Account, TCHAR* Text, INT Flag) NW_STOCK(0x1006ca90)
#endif

// (NChatInviteWnd is declared in MsnUiStockP.h)

// 0x1013e330 (name ours) - InviteWnd: its list of friends emptied (virtual 71)
void NChatInviteWnd::ClearList()
{
    FriendList->Clear();
}
NW_HOOK_METHOD(0x1013e330, NChatInviteWnd, ClearList)

struct NChatPeerEx : public NChatPeer
{
    void Destroy();   // 0x10119C20
};

// 0x1013e4c0 (name ours) - a person of a conversation (NChatPeer) destroyed: its user at +8
void NChatPeerEx::Destroy()
{
    User.~NChatUser();
}
NW_HOOK_METHOD(0x1013e4c0, NChatPeerEx, Destroy)

struct NChatWord34Getter
{
    INT GetWord34();   // 0x1014bbd0
};

// 0x1014bbd0 (name ours; the db's MSNSocket::GetOwner is the C3 map's guess: the C4 body reads +0x34, not the owner at +0x38) - the number
// at +0x34, a getter folded with the ones of other classes; MSNSocket::OnSocketMessage calls it on a MsnLine for bAsync
INT NChatWord34Getter::GetWord34()
{
    return ::nw::Field<INT>(this, 0x34);
}
NW_HOOK_METHOD(0x1014bbd0, NChatWord34Getter, GetWord34)

#if NW_HOOK_MODE
struct NChatCommandGetter : public MsnCommand
{
    INT GetWord0C();   // 0x1012C6B0
};

// 0x10151150  NOT PORTED (not in eligible.csv (category D or not a game function)); nothing calls it, the standalone has no use for the stub
NW_NAKED INT NChatCommandGetter::GetWord0C() NW_STOCK(0x10151150)
#endif

struct NChatSelfGetter
{
    NChatSelfGetter* GetSelf();   // 0x1012DC70
};

// 0x10152850 (name ours) - the object itself (a getter folded with the ones of other classes)
NChatSelfGetter* NChatSelfGetter::GetSelf()
{
    return this;
}
NW_HOOK_METHOD(0x10152850, NChatSelfGetter, GetSelf)

/*-----------------------------------------------------------------------------
    The header map's lookup.
-----------------------------------------------------------------------------*/
struct NChatHeaderMap : public NMsnHeaderMap
{
    FString Lookup(const FString& Key);   // 0x101279C0 (ret 8: the hidden result, the key)
};

// 0x1014c490 (ret 8) - the value of the header (case-insensitive key, hashed by NStrihash), a copy; an empty string when there is none
FString NChatHeaderMap::Lookup(const FString& Key)
{
    for (INT i = Hash[NStrihash(*Key) & (HashCount - 1)]; i != -1; i = Pairs(i).HashNext)
    {
        if (Pairs(i).Key == Key)
            return Pairs(i).Value;
    }
    return FString();
}
NW_HOOK_METHOD(0x1014c490, NChatHeaderMap, Lookup)

/*-----------------------------------------------------------------------------
    The task lists.
-----------------------------------------------------------------------------*/
// (NChatTaskListBox is declared in MsnUiStockP.h)

// 0x10149a70 (ret 4, name ours; TaskBox's and L2TaskBox's, the linker folded them) - the line whose Param is that task window leaves the
// list (0x1012ca10: the array's Remove of one 12-byte line), and the counts of lines (+0x204) and of the first one shown (+0x200) go
// down by one when they are above 0
void NChatTaskListBox::RemoveTask(NCWnd* Task)
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (Items(i).Param == (INT)Task)
        {
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x101495b0, &Items, i, 1);
#else
            Items.Remove(i, 1);                           // TListItemArray::Remove(i, 1): the standalone compiles the template
#endif
            if (NumLines > 0)
                NumLines--;
            if (TopIndex > 0)
                TopIndex--;
            return;
        }
    }
}
NW_HOOK_METHOD(0x10149a70, NChatTaskListBox, RemoveTask)

// (NChatTaskWnd is declared in MsnUiStockP.h)

// 0x10141c90 (name ours; the db called it MsnWnd::PrepareLastWindow) - TaskWnd: every task window leaves the list box and the window and
// is deleted; the map starts again with 8 hash entries
void NChatTaskWnd::DestroyTasks()
{
    for (INT i = 0; i < Tasks.Pairs.Num(); i++)
    {
        NCWnd* Task = Tasks.Pairs(i).Value;
        if (Task)
        {
            ((NChatTaskListBox*)List)->RemoveTask(Task);   // 0x10149A70
            RemoveChildWindow(Task->Entry);
            delete Task;
        }
    }
    Tasks.Pairs.FArray::Empty(sizeof(NTaskPair));
    Tasks.HashCount = 8;
    Tasks.Rehash();
}
NW_HOOK_METHOD(0x10141c90, NChatTaskWnd, DestroyTasks)

struct NChatL2TaskInit : public L2Task
{
    L2Task* InitWith(FString InName, NCWnd* InChatWindow, INT InAlert);   // 0x10125EF0 (ret 0x14)
};

// 0x1014a9c0 (ret 0x14, name ours) - the constructor of a Lineage II task entry on memory the allocation made: (friend's name by value,
// the conversation window, the alert flag). The default constructor's work (base, vtable, fields cleared), then the three values; the
// name's copy is destroyed on return, and the object comes back in eax
L2Task* NChatL2TaskInit::InitWith(FString InName, NCWnd* InChatWindow, INT InAlert)
{
    L2Task* Task = ::new ((void*)this) L2Task;
    Task->Name = InName;
    Task->ChatWindow = InChatWindow;
    Task->bAlert = InAlert;
    return Task;
}
typedef L2Task* (NChatL2TaskInit::*SigChatL2TaskInit)(FString, NCWnd*, INT);
NW_HOOK_VARARGS(0x1014a9c0, NChatL2TaskInit, InitWith, SigChatL2TaskInit)

// (NChatL2TaskWnd is declared in MsnUiStockP.h)

// 0x1014ad00 (ret 0x14, name ours) - the entry of a friend's conversation in the list: the one that already has this friend's name is
// told whether a message waits (when the conversation window is not shown), otherwise a new L2Task is made (style 1, at 0, 0),
// entered under the conversation window and its line added to the list (the name, the task as the line's Param, the usual color)
void NChatL2TaskWnd::AddTask(NCWnd* ChatWindow, FString Name, INT bAlert)
{
    L2Task* Task = static_cast<L2Task*>(FindTask(Name));
    if (!Task)
    {
        // the L2Task made as the stock does: the object allocated, then its constructor 0x1014A9C0 (InitWith above) run on it
        void* TaskMem = UObject::StaticAllocateObject(L2Task::StaticClass());
        Task = TaskMem ? ((NChatL2TaskInit*)TaskMem)->InitWith(Name, ChatWindow, bAlert) : NULL;
        Task->SetPosition(0, 0, 0, 0);
        CreateChild(Task, 1);
        NCWnd* TaskWindow = Task;
        ((NMsnTaskMapOps*)&Tasks)->SetTask(&ChatWindow, &TaskWindow);           // the map: conversation window -> task, 0x10140D80
        List->AddString(*Name, (DWORD)Task, 0xFFDCDCDC);
    }
    else if (!ChatWindow->IsShowWindow())
        Task->bAlert = bAlert;
}
typedef void (NChatL2TaskWnd::*SigChatAddTask)(NCWnd*, FString, INT);
NW_HOOK_VARARGS(0x1014ad00, NChatL2TaskWnd, AddTask, SigChatAddTask)

/*-----------------------------------------------------------------------------
    The messenger's text replacer.
-----------------------------------------------------------------------------*/

// 0x10168890 (cdecl; the two strings by value, destroyed on return) - every From in Str is replaced by To. The stock cuts one character too
// many after each match and keeps the whole rest (match included) when the match is at the end or one before it: reproduced
static void __cdecl NMsnStrReplaceImpl(FString* Str, FString From, FString To)
{
    if (Str->Len() > 0)
    {
        FString Result;
        INT FromLen = From.Len();
        INT Pos = Str->InStr(*From, 0);
        while (Pos != -1)
        {
            Result += Str->Left(Pos);
            Result += To;
            if (Str->Len() <= Pos + FromLen + 1)
                break;
            *Str = Str->Right(Str->Len() - Pos - FromLen - 1);
            Pos = Str->InStr(*From, 0);
        }
        if (Str->Len() > 0)
            Result += *Str;
        *Str = Result;
    }
}
NW_HOOK_FUNC(0x10168890, NMsnStrReplaceImpl)

// Not hooked on purpose: the MD5 transform 0x10126700 and its two helpers 0x10126680 / 0x101266C0 take their arguments in eax / ecx /
// edx (the stock was built with whole-program optimization); regcheck rejects every hook that reads a register argument. Their only
// callers are the stock Update / Final (0x10126FC0 / 0x10127080), hooked by NChatMsg.cpp's NMd5Update / NMd5Final, which run the
// transform in C++ (NMd5Transform): the three stock bodies are dead in hook mode (db/morto.csv).

NW_CHECK_VTABLE(ChatWnd, 0x101d9d78, 74)
