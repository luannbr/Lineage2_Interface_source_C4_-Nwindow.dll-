/*=============================================================================
    MsnWnd.cpp - the messenger's main window (C3 only; C3 addresses). Stock: 0x10130E30 .. 0x10130F20 (small calls), 0x10131070 ..
    0x101311D0 (the protocol object's notices), 0x101316B0, 0x10132BE0 (OnPaint), 0x101332C0 .. 0x10133A20 (the dialogs and the
    buttons), 0x10136740 / 0x10136B60 (ShowWindow, HideWindow), 0x10137A60 (OnDestroy), 0x10137F50 (OnTimer), 0x10138AA0
    (double click), 0x10139700, 0x10139A40 (OnCreate), 0x1013AA20 (sign out), 0x1013AD30 (WindowProc), 0x1013AE40 / 0x1013AFE0
    (constructor, destructor), 0x10144050 / 0x10144150 (the text fitters).
    Left stock on purpose: the protocol object (Msn: 0x1012C180 / 0x1012BF80 and its calls) and the chat windows' helpers
    (0x101389F0, 0x10138520) belong to the protocol and chat lanes. Wave 5 added ShowError 0x10133BE0, OnSessionRemoved 0x10138460 and the
    MoveGroupDialog setter 0x10131600 here (ChatWnd::ResetSession 0x1011E330 is ChatWnd.cpp's).
=============================================================================*/
#include "nwindow/MsnWnd.h"
#include "nwindow/MsnUiStockP.h"
#include "nwindow/AlarmWnd.h"
#include "nwindow/ChatWnd.h"
#include "nwindow/InviteWnd.h"
#include "nwindow/LoadingWnd.h"
#include "nwindow/LoginWnd.h"
#include "nwindow/MessengerWnd.h"
#include "nwindow/MsnTree.h"
#include "nwindow/MsnTreeCtrl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/NL2Font.h"
#include "nwindow/NMsnTree.h"
#include "nwindow/OptionWnd.h"
#include "nwindow/StatusWnd.h"
#include "nwindow/Task.h"
#include "nwindow/TaskWnd.h"
#include "nwindow/SwitchboardSession.h"
#include "nwindow/VersionWnd.h"

// the global the messenger's buttons read before they act (0x102C3200; nothing in this image writes it)
#if NW_HOOK_MODE
#define GMsnBlocked NW_GLOBAL(INT, 0x10322C68)
#else
extern INT GMsnBlocked;                 // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

enum
{
    MSN_StatusButtonID = 0xA52,
    MSN_AddFriendID = 0xA04,
    MSN_RemoveFriendID = 0xA05,
    MSN_BlockID = 0xA06,
    MSN_MoveGroupID = 0xA07,
    MSN_SignOutID = 0xA08,
    MSN_SignOutTimerID = 0xA55,
    MSN_SignOutDelay = 5000,
    SYSMSG_MsnBlocked = 1309,       // the messenger is blocked
    SYSMSG_NoFriendToRemove = 1362,
    SYSMSG_NoFriendToBlock = 1318,
    SYSMSG_NoFriendToMove = 1319,
};

// storage for an FString that a stock function builds (its hidden return slot): no constructor of ours runs on it
struct NMsnRawFString
{
    BYTE Raw[sizeof(FString)];
    FString* Get() { return (FString*)Raw; }
};

// The protocol object's flags: +0x4C is set while the sign-in is done (the window's layout depends on it)
#define MsnLoggedIn(Msn) (::nw::Field<INT>((Msn), 0x4C))

#if NW_HOOK_MODE
// An object made as the stock code makes it: StaticAllocateObject of its class, then the constructor at Ctor of this client (the hook build
// keeps it for ChatWnd's constructor with by-value arguments, see OpenChat; the standalone makes it with `new`)
template <class T, class... A> static FORCEINLINE T* NewStockObj(UClass* Class, DWORD Ctor, A... a)
{
    void* Mem = UObject::StaticAllocateObject(Class);
    return Mem ? ::nw::StockCallAt<T*>(Ctor, Mem, a...) : NULL;
}
#endif

// MoveGroupDialog.h cannot be included next to the protocol headers (the two lanes' NMsnCommand / NMsnGroup clash), so the dialog is
// seen through a bare struct: only `this` and the person record at +0x1F8 are used
struct NMoveGroupDialogSetter
{
    __declspec(safebuffers) void SetPerson(NMsnPersonRec Person);   // 0x10131600 (ret 0x44)
};

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// A button of the window (repeated inline in the stock OnCreate): the textures, the id and the place; the label comes after
static FORCEINLINE NCButton* CreateMsnButton(NCWnd* Parent, const TCHAR* Normal, const TCHAR* Pushed, INT ID, INT X, INT Y, INT W, INT H)
{
    NCButton* Button = new NCButton(Normal, Pushed, 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, W, H);
    Parent->CreateChild(Button, NWS_VISIBLE);
    return Button;
}

/*-----------------------------------------------------------------------------
    What MsnWnd.h declares out of line: the person and group records (the protocol lane's NMsnFriend / NMsnGroup, the same bytes), the
    calls of the protocol object, and the bridges to the dialogs' files.
-----------------------------------------------------------------------------*/

// 0x101510d0 - NMsnFriend::GetDisplayName builds the name (ret 4: the hidden result) in Out, which is raw memory
__declspec(noinline) FString* NMsnPersonRec::GetDisplayName(FString* Out)
{
    new ((void*)Out) FString(((NMsnFriend*)this)->GetDisplayName());
    return Out;
}

// 0x10151070 - NMsnFriend::operator=
__declspec(noinline) void NMsnPersonRec::Assign(const NMsnPersonRec* Src)
{
    *(NMsnFriend*)this = *(const NMsnFriend*)Src;
}

// 0x10150f20 - NMsnFriend's copy constructor on the record being returned
__declspec(noinline) NMsnPersonRec MsnPersonCopy(const NMsnPersonRec& Src)
{
    NMsnPersonRec Copy;
    new ((void*)&Copy) NMsnFriend(*(const NMsnFriend*)&Src);
    return Copy;
}

// 0x10150ff0 - ~NMsnFriend
__declspec(noinline) void MsnPersonFree(NMsnPersonRec* Rec)
{
    ((NMsnFriend*)Rec)->~NMsnFriend();
}

// 0x1013e9a0 - NChatUser's constructor (five empty strings, the numbers left alone); the destructor is ~NMsnFriend (0x10150ff0)
__declspec(noinline) NMsnPerson::NMsnPerson()
{
    new ((void*)this) NChatUser;
}
__declspec(noinline) NMsnPerson::~NMsnPerson()
{
    ((NMsnFriend*)this)->~NMsnFriend();
}

// 0x101526e0 / 0x10152760 - NMsnGroup's copy constructor and destructor
__declspec(noinline) NMsnGroupArg MsnGroupCopy(const NMsnGroupArg& Src)
{
    NMsnGroupArg Copy;
    new ((void*)&Copy) NMsnGroup(*(const NMsnGroup*)&Src);
    return Copy;
}
__declspec(noinline) void MsnGroupFree(NMsnGroupArg* Rec)
{
    ((NMsnGroup*)Rec)->~NMsnGroup();
}

// 0x1014f550 / 0x1014f5c0 / 0x10150120 / 0x1014f300 - the protocol object's SetStatus, SetDisplayName, SignIn and Block (the strings by value)
__declspec(noinline) void MsnSetStatusArg(Msn* M, NMsnStringArg Code)
{
    M->SetStatus(Code);
}
__declspec(noinline) void MsnSetDisplayNameArg(Msn* M, NMsnStringArg Name)
{
    M->SetDisplayName(Name);
}
__declspec(noinline) void MsnSignIn(Msn* M, NMsnStringArg Account, NMsnStringArg Password)
{
    M->SignIn(Account, Password);
}
__declspec(noinline) void MsnBlockArg(Msn* M, NMsnStringArg Account, INT bBlock)
{
    M->Block(Account, bBlock);
}

// 0x101515e0 - FriendList::FindFriend: the person record of that account in the list, NULL for none (the account by value)
__declspec(noinline) void* MsnFindInList(void* List, NMsnStringArg Account)
{
    void* Found = ((FriendList*)List)->FindFriend(Account.Str());     // a copy goes in the argument; the callee's own is freed here
    Account.Str().~FString();
    return Found;
}

// the small getters (0x1014fc10 [M + 0x50], 0x1014fc20 [M + 0x54], 0x10005b90 [Lists + 8]) and Msn::FindFriend (0x1014fc30)
__declspec(noinline) void* MsnGetLists(Msn* M)
{
    return (void*)(INT_PTR)((NFoldedDwordAt50*)M)->Get();
}
__declspec(noinline) NMsnPersonRec* MsnGetSelf(Msn* M)
{
    return (NMsnPersonRec*)((NChatMsnRequester*)M)->GetMe();
}
__declspec(noinline) void* MsnListBlocked(void* Lists)
{
    return (void*)(INT_PTR)((NFoldedDwordAt08*)Lists)->Get();
}
__declspec(noinline) NMsnPersonRec* MsnFindFriendArg(Msn* M, NMsnStringArg Account)
{
    NMsnPersonRec* Found = (NMsnPersonRec*)M->FindFriend(Account.Str());     // a copy goes in the argument; the callee's own is freed here
    Account.Str().~FString();
    return Found;
}

// 0x10089d40 - NMsnIntMap::Rehash (MSNSocket.cpp): the number table of the window has that layout
__declspec(noinline) void NMsnWndIntMap::Rehash()
{
    ((NMsnIntMap*)this)->Rehash();
}

// what the dialog of "appear offline" asks of the window (OfflineConfirmDialog.cpp cannot include the protocol headers)
__declspec(noinline) void MsnChatResetSession(NCWnd* Chat)
{
    static_cast<ChatWnd*>(Chat)->ResetSession();                                  // 0x10142EC0
}
__declspec(noinline) void MsnTaskWndDestroyTasks(NCWnd* TaskWindow)
{
    ((NChatTaskWnd*)static_cast<TaskWnd*>(TaskWindow))->DestroyTasks();            // 0x10141C90
}

/*-----------------------------------------------------------------------------
    The text fitters: a line cut to a width, and a text cut into lines for a box.
-----------------------------------------------------------------------------*/

// 0x10168a10 (name ours; cdecl) - the text up to the width MaxWidth (in the normal font) goes into Out, with "..." added when
// it does not all fit and EllipsisReserve is positive (that many pixels are kept free for the dots); a line break ends it.
// The result is the number of characters taken (a break and the dots are not counted). C4 against C3: the same code (the stock
// reads the canvas' L2Fonts[0] at +0x13C and the glyph through the font's vslot 28, whose Width is at +4); the stock stores the
// three dots one by one, the compiler makes two stores of them (verify_data: const 0x2e002e / 0x13c: see db/verify_data_notes.csv)
__declspec(noinline) INT MsnFitLine(const TCHAR* Text, TCHAR* Out, INT MaxWidth, INT EllipsisReserve)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, (L2FontType)0);
    if (Size.cx <= MaxWidth)
    {
        appStrcpy(Out, Text);
        return appStrlen(Out);
    }
    INT Taken = 0;
    INT Pos = 0;
    INT Width = 0;
    UL2Font* Font = GCanvas->L2Fonts[0];
    for (TCHAR Char = *Text; Char != 0; Char = *Text)
    {
        Text++;
        if (Char == '\n')
        {
            Out[Pos++] = '\n';
            break;
        }
        FL2FontChar* Glyph = Font->GetChar(Char);
        if (Glyph)
        {
            Width += Glyph->Width;
            if (Width > MaxWidth - EllipsisReserve)
                break;
            Out[Pos++] = Char;
            Taken++;
        }
    }
    if (EllipsisReserve > 0)
    {
        Out[Pos++] = '.';
        Out[Pos++] = '.';
        Out[Pos++] = '.';
    }
    Out[Pos] = 0;
    return Taken;
}
NW_HOOK_FUNC(0x10168a10, MsnFitLine)

// 0x10168b10 (name ours; cdecl) - the text cut into lines of at most MaxWidth pixels, as many as fit MaxHeight (the height of
// the letter A in the normal font is a line), the last of them ending in "..." when the text goes on; the lines are
// appended to Out. C4: the same code as C3
void MsnFitText(const TCHAR* Text, TCHAR* Out, INT MaxWidth, INT MaxHeight)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, MaxWidth, 0, 0, 0, 0, (L2FontType)0);
    SIZE Letter;
    NCGDevice::GetCharSize(&Letter, 0x41, 0);
    INT Lines = Size.cy / Letter.cy;
    INT MaxLines = MaxHeight / Letter.cy;
    if (Lines > MaxLines)
        Lines = MaxLines;
    TCHAR Line[260];
    INT Pos = 0;
    for (INT i = 0; i < Lines; i++)
    {
        Pos += MsnFitLine(Text + Pos, Line, MaxWidth, i == Lines - 1 ? 0xC : 0);
        appStrcat(Out, Line);
    }
}
NW_HOOK_FUNC(0x10168b10, MsnFitText)

/*-----------------------------------------------------------------------------
    MsnWnd.
-----------------------------------------------------------------------------*/

// 0x1015f820 - the maps and the person record are built by their members' constructors; the pointers are cleared
MsnWnd::MsnWnd()
{
    MsnObj = NULL;
    SignOutTimer = 0;
    LoginWindow = NULL;
    VersionWindow = NULL;
    LoadingWindow = NULL;
    TreeCtrl = NULL;
    L2FriendWindow = NULL;
    OptionWindow = NULL;
    AddFriendDialog = NULL;
    RemoveFriendDialog = NULL;
    MoveGroupDialog = NULL;
    AddedConfirmDialog = NULL;
    OfflineConfirmDialog = NULL;
    AddGroupDialog = NULL;
    RenameGroupDialog = NULL;
    RemoveGroupDialog = NULL;
    MessageDialog = NULL;
    AlarmWindow = NULL;
    StatusWindow = NULL;
    StatusButton = NULL;
    AddFriendButton = NULL;
    RemoveFriendButton = NULL;
    MoveGroupButton = NULL;
    AddGroupButton = NULL;
    SignOutButton = NULL;
    BackTex = NULL;
}
NW_HOOK_CTOR(0x1015f820, MsnWnd)

// 0x1015f9c0 - the person record and the three maps are freed by the members' destructors (in reverse order)
NW_HOOK_DTOR(0x1015f9c0, MsnWnd)

// 0x1015f710 (vslot 25) - a double click in the tree, the buttons
INT MsnWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0x203)
        return OnDoubleClick((INT)wParam, (INT)lParam) != 0;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == MSN_SignOutID)
            return OnSignOutButton() != 0;
        if (wParam == MSN_StatusButtonID)
            return OnStatusButton() != 0;
        if (wParam == MSN_AddFriendID)
            return OnAddFriendButton() != 0;
        if (wParam == MSN_RemoveFriendID)
            return OnRemoveFriendButton() != 0;
        if (wParam == MSN_BlockID)
            return OnBlockButton() != 0;
        if (wParam == MSN_MoveGroupID)
            return OnMoveGroupButton() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1015f710, MsnWnd, WindowProc)

// 0x1015e3b0 (vslot 37) - NCWnd's, the picture, the protocol object, then the windows: the log-in, version and loading
// windows over the whole window, the person tree, the options window and the nine dialogs (children of the desktop, in the
// middle of the screen), the alarm box (the bottom right corner), the status menu, and the six buttons
INT MsnWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.msn_back0"), 1);
    void* MsnMem = appMalloc(0x58, TEXT("new"));
#if NW_HOOK_MODE
    MsnObj = MsnMem ? ((Msn*)MsnMem)->Construct(this) : NULL;            // the protocol object, 0x10150C40
#else
    MsnObj = MsnMem ? new (MsnMem) Msn(this) : NULL;                       // the real constructor (the compiler writes our vtable)
#endif

    LoginWindow = new LoginWnd(this);
    LoginWindow->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(LoginWindow, NWS_VISIBLE);
    VersionWindow = new VersionWnd(this);
    VersionWindow->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(VersionWindow, NWS_VISIBLE);
    LoadingWindow = new LoadingWnd(this);
    LoadingWindow->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(LoadingWindow, NWS_VISIBLE);
    TreeCtrl = new MsnTreeCtrl();
    TreeCtrl->SetPosition(8, 0x2D, 0xF0, 0x10E);
    CreateChild(TreeCtrl, NWS_VISIBLE);

    NCWnd* Desktop = GConsole->Desktop;
    OptionWindow = new OptionWnd(this, MsnObj);
    OptionWindow->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x96, 0x100, 0x13E);
    Desktop->CreateChild(OptionWindow, 0x4002);
    ::nw::VirtualCall<void>(OptionWindow, 56, 1001);                // SetTitleID(1001): the C4 stock calls the slot 56 (0xE0) with the SysString id

    AddFriendDialog = MsnNewAddFriendDialog(MsnObj);                    // (the dialogs are made by their own files: see MsnWnd.h)
    AddFriendDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(AddFriendDialog, 0x2);
    RemoveFriendDialog = MsnNewRemoveFriendDialog(this, MsnObj);
    RemoveFriendDialog->SetPosition(GSEKScreenX / 2 - 0x9C, GSEKScreenY / 2 - 0x48, 0x138, 0x91);
    Desktop->CreateChild(RemoveFriendDialog, 0x2);
    MoveGroupDialog = MsnNewMoveGroupDialog(MsnObj);
    MoveGroupDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(MoveGroupDialog, 0x12);
    AddedConfirmDialog = MsnNewAddedConfirmDialog(MsnObj);
    AddedConfirmDialog->SetPosition(GSEKScreenX / 2 - 0xD2, GSEKScreenY / 2 - 0x54, 0x1A4, 0xA8);
    Desktop->CreateChild(AddedConfirmDialog, 0x22);
    OfflineConfirmDialog = MsnNewOfflineConfirmDialog(this);
    OfflineConfirmDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(OfflineConfirmDialog, 0x22);
    AddGroupDialog = MsnNewAddGroupDialog(MsnObj);
    AddGroupDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(AddGroupDialog, 0x2);
    RenameGroupDialog = MsnNewRenameGroupDialog(MsnObj);
    RenameGroupDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(RenameGroupDialog, 0x12);
    RemoveGroupDialog = MsnNewRemoveGroupDialog(this, MsnObj);
    RemoveGroupDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(RemoveGroupDialog, 0x12);
    MessageDialog = MsnNewMessageDialog();
    MessageDialog->SetPosition(GSEKScreenX / 2 - 0x80, GSEKScreenY / 2 - 0x36, 0x100, 0x6C);
    Desktop->CreateChild(MessageDialog, 0x12);

    AlarmWindow = new AlarmWnd(this, NULL);
    AlarmWindow->SetPosition(GSEKScreenX - 0xAC, GSEKScreenY, 0xAC, 0x6A);
    Desktop->CreateChild(AlarmWindow, NWS_VISIBLE);
    StatusWindow = new StatusWnd(this, MsnObj);
    StatusWindow->SetPosition(9, 0x2D, 0x96, 0x90);
    CreateChild(StatusWindow, NWS_VISIBLE);

    StatusButton = CreateMsnButton(this, TEXT("L2UI_CH3.Msn.msnbutton2"), TEXT("L2UI_CH3.Msn.msnbutton2"), MSN_StatusButtonID, 0x24, 0x1B, 0xB, 0xB);
    AddFriendButton = CreateMsnButton(this, TEXT("L2UI_CH3.Button.smallbutton1"), TEXT("L2UI_CH3.Button.smallbutton1_down"), MSN_AddFriendID, 0xB, 0x144, 0x31, 0x17);
    AddFriendButton->SetTextID(654);
    RemoveFriendButton = CreateMsnButton(this, TEXT("L2UI_CH3.Button.smallbutton1"), TEXT("L2UI_CH3.Button.smallbutton1_down"), MSN_RemoveFriendID, 0x40, 0x144, 0x31, 0x17);
    RemoveFriendButton->SetTextID(425);
    MoveGroupButton = CreateMsnButton(this, TEXT("L2UI_CH3.Button.smallbutton1"), TEXT("L2UI_CH3.Button.smallbutton1_down"), MSN_BlockID, 0x75, 0x144, 0x31, 0x17);
    MoveGroupButton->SetTextID(993);
    AddGroupButton = CreateMsnButton(this, TEXT("L2UI_CH3.BUTTON.Btn1_normal"), TEXT("L2UI_CH3.BUTTON.Btn1_normalOn"), MSN_MoveGroupID, 0xAB, 0x144, 0x4C, 0x17);
    AddGroupButton->SetTextID(994);
    SignOutButton = CreateMsnButton(this, TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), MSN_SignOutID, 0xB8, 0x14, 0x42, 0x15);
    SignOutButton->SetTextID(998);
    return 1;
}
NW_HOOK_METHOD(0x1015e3b0, MsnWnd, OnCreate)

// 0x1015c3d0 (vslot 38) - the protocol object is destroyed and freed; the chat windows leave the desktop and the maps are
// emptied; the log-in, version, loading, tree and friends windows go (children of this window); the options window, the
// dialogs and the alarm box only leave the desktop; the status menu and the six buttons go; then NCWnd's
INT MsnWnd::OnDestroy()
{
    if (MsnObj)
    {
        MsnObj->Destroy();                          // the protocol object's destructor, 0x10150A40
        appFree(MsnObj);
        MsnObj = NULL;
    }
    NCWnd* Desktop = GConsole->Desktop;
    for (INT i = 0; i < ChatWnds.Num; i++)
    {
        if (ChatWnds.Data[i].Wnd)
            Desktop->RemoveChildWindow(ChatWnds.Data[i].Wnd->Entry);
    }
    ChatWnds.Reset();
    ChatNames.Reset();
    InviteWnds.Reset();
    if (SignOutTimer)
    {
        KillTimer(SignOutTimer);
        SignOutTimer = 0;
    }
    DestroyChild(this, LoginWindow);
    DestroyChild(this, VersionWindow);
    DestroyChild(this, LoadingWindow);
    DestroyChild(this, TreeCtrl);
    DestroyChild(this, L2FriendWindow);
    Desktop->RemoveChildWindow(OptionWindow->Entry);
    Desktop->RemoveChildWindow(AddFriendDialog->Entry);
    Desktop->RemoveChildWindow(RemoveFriendDialog->Entry);
    Desktop->RemoveChildWindow(MoveGroupDialog->Entry);
    Desktop->RemoveChildWindow(AddedConfirmDialog->Entry);
    Desktop->RemoveChildWindow(OfflineConfirmDialog->Entry);
    Desktop->RemoveChildWindow(AddGroupDialog->Entry);
    Desktop->RemoveChildWindow(RenameGroupDialog->Entry);
    Desktop->RemoveChildWindow(RemoveGroupDialog->Entry);
    Desktop->RemoveChildWindow(MessageDialog->Entry);
    Desktop->RemoveChildWindow(AlarmWindow->Entry);
    DestroyChild(this, StatusWindow);
    DestroyChild(this, StatusButton);
    DestroyChild(this, AddFriendButton);
    DestroyChild(this, RemoveFriendButton);
    DestroyChild(this, MoveGroupButton);
    DestroyChild(this, AddGroupButton);
    DestroyChild(this, SignOutButton);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x1015c3d0, MsnWnd, OnDestroy)

// 0x1015b130 (vslot 44) - NCWnd's, the windows by number hidden; signed in: the tree, the six buttons and the chat windows
// that are not minimized (and the friends window when it has windows) shown, everything else hidden; signed out: only the
// log-in window shown
void MsnWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    for (INT i = 0; i < InviteWnds.Num; i++)
    {
        if (InviteWnds.Data[i].Wnd)
            InviteWnds.Data[i].Wnd->HideWindow();
    }
    if (MsnLoggedIn(MsnObj))
    {
        LoginWindow->HideWindow();
        VersionWindow->HideWindow();
        LoadingWindow->HideWindow();
        OptionWindow->HideWindow();
        AlarmWindow->HideWindow();
        StatusWindow->HideWindow();
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        AddedConfirmDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        TreeCtrl->ShowWindow();
        StatusButton->ShowWindow();
        AddFriendButton->ShowWindow();
        RemoveFriendButton->ShowWindow();
        MoveGroupButton->ShowWindow();
        AddGroupButton->ShowWindow();
        SignOutButton->ShowWindow();
        for (INT i = 0; i < ChatWnds.Num; i++)
        {
            NCWnd* Chat = ChatWnds.Data[i].Wnd;
            if (Chat && static_cast<ChatWnd*>(Chat)->bHidden == 0)
                Chat->ShowWindow();
        }
        if (L2FriendWindow && static_cast<TaskWnd*>(L2FriendWindow)->Tasks.Pairs.Num() > 0)
            L2FriendWindow->ShowWindow();
    }
    else
    {
        VersionWindow->HideWindow();
        LoadingWindow->HideWindow();
        TreeCtrl->HideWindow();
        if (L2FriendWindow)
            L2FriendWindow->HideWindow();
        OptionWindow->HideWindow();
        AlarmWindow->HideWindow();
        StatusWindow->HideWindow();
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        AddedConfirmDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        StatusButton->HideWindow();
        AddFriendButton->HideWindow();
        RemoveFriendButton->HideWindow();
        MoveGroupButton->HideWindow();
        AddGroupButton->HideWindow();
        SignOutButton->HideWindow();
        LoginWindow->ShowWindow();
    }
}
NW_HOOK_METHOD(0x1015b130, MsnWnd, ShowWindow)

// 0x1015b550 (vslot 45) - NCWnd's; with "hide all" set the chat windows and the friends window go too; the options window,
// the alarm box and the dialogs (but the status menu) always
// The stock ends HideWindow with a call and a ret (it keeps an exception frame around the map iterator it walks); ours indexes the map, and the
// optimizer would end with a tail jump into the last dialog's virtual HideWindow, which the vtable check (slot 49) reads as another function.
// A local with a destructor keeps the last call a call.
static volatile INT GMsnNoTail;
struct NMsnNoTail
{
    ~NMsnNoTail() { GMsnNoTail = 0; }
};

void MsnWnd::HideWindow()
{
    NMsnNoTail NoTail;
    NCWnd::HideWindow();
    if (GMsnHideAll)
    {
        for (INT i = 0; i < ChatWnds.Num; i++)
        {
            if (ChatWnds.Data[i].Wnd)
                ChatWnds.Data[i].Wnd->HideWindow();
        }
        if (L2FriendWindow)
            L2FriendWindow->HideWindow();
    }
    OptionWindow->HideWindow();
    AlarmWindow->HideWindow();
    AddFriendDialog->HideWindow();
    RemoveFriendDialog->HideWindow();
    MoveGroupDialog->HideWindow();
    AddedConfirmDialog->HideWindow();
    OfflineConfirmDialog->HideWindow();
    AddGroupDialog->HideWindow();
    RenameGroupDialog->HideWindow();
    RemoveGroupDialog->HideWindow();
    MessageDialog->HideWindow();
}
NW_HOOK_METHOD(0x1015b550, MsnWnd, HideWindow)

// 0x1015c8c0 (vslot 36) - the sign-out timer (5 s): the timer stops, the tree is emptied, every chat window is reset and
// hidden and remembered in the map by number, the maps are emptied, the friends window is cleared, the loading box goes and,
// when the console is in its state 6 and this window is shown, the log-in window comes back
INT MsnWnd::OnTimer(INT TimerID)
{
    if (TimerID == MSN_SignOutTimerID)
    {
        if (SignOutTimer)
        {
            KillTimer(SignOutTimer);
            SignOutTimer = 0;
        }
        TreeCtrl->Clear();
        for (INT i = 0; i < ChatWnds.Num; i++)
        {
            NCWnd* Chat = ChatWnds.Data[i].Wnd;
            if (Chat)
            {
                static_cast<ChatWnd*>(Chat)->ResetSession();                       // the chat window forgets its session, 0x10142EC0
                INT Key = (INT)Chat;
                NCWnd* Value = Chat;
                InviteWnds.SetWnd(&Key, &Value);                                     // InviteWnds.Set(Chat, Chat), 0x10151770
                Chat->HideWindow();
            }
        }
        ChatWnds.Reset();
        ChatNames.Reset();
        if (L2FriendWindow)
            ((NChatTaskWnd*)L2FriendWindow)->DestroyTasks();                       // the friends window closes its windows, 0x10141C90
        LoadingWindow->HideWindow();
        if (GConsole->State == 6 && IsShowWindow())
            LoginWindow->ShowWindow();
    }
    return 1;
}
NW_HOOK_METHOD(0x1015c8c0, MsnWnd, OnTimer)

// 0x10157870 (vslot 56) - signed in: the picture, the big presence icon at (9, 10) with the presence's name in brackets at
// (52, 27), and the person's own name (cut to 128 pixels) at (52, 11)
INT MsnWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (MsnLoggedIn(MsnObj))
    {
        if (BackTex)
            Device->DrawTexture(0, 0, 0x100, 0x161, 0, 0, 0x100, 0x161, BackTex, 0xFF, 1);
        FString Presence = MsnObj->GetStatus();                                                   // the presence code, Msn::GetStatus 0x1014F1D0
        const TCHAR* IconName = NULL;
        INT TextID = 0;
        if (Presence == TEXT("NLN"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon1");
            TextID = 1015;
        }
        else if (Presence == TEXT("BRB"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon2");
            TextID = 1017;
        }
        else if (Presence == TEXT("IDL") || Presence == TEXT("AWY"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon2");
            TextID = 1018;
        }
        else if (Presence == TEXT("LUN"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon2");
            TextID = 1020;
        }
        else if (Presence == TEXT("BSY"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon3");
            TextID = 1016;
        }
        else if (Presence == TEXT("PHN"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon3");
            TextID = 1019;
        }
        else if (Presence == TEXT("FLN") || Presence == TEXT("HDN"))
        {
            IconName = TEXT("L2UI_CH3.Msn.msnbigicon4");
            TextID = 1021;
        }
        if (IconName)
        {
            Device->DrawTexture(9, 0xA, 0x1D, 0x1D, 0, 0, 0x1D, 0x1D, FindTexture(IconName, 1), 0xFF, 1);
            FString Shown = FString::Printf(TEXT("(%s)"), *MSN_SYSSTR(TextID));
            Device->DrawText(0x34, 0x1B, 0xFFB09B79, *Shown, 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
        }
        TCHAR Name[260];
        {
            NMsnRawFString AccountRaw;
            FString& Account = *Me.GetDisplayName(AccountRaw.Get());
            MsnFitLine(*Account, Name, 0x80, 0xC);
            Account.~FString();
        }
        Device->DrawText(0x34, 0xB, 0xFFDCDCDC, Name, 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10157870, MsnWnd, OnPaint)

/*-----------------------------------------------------------------------------
    The protocol object's notices: a person or a group arrives, a person goes.
-----------------------------------------------------------------------------*/

// 0x10155ce0 (ret 0x44) - the tree takes a copy of the person
void MsnWnd::NotifyAddFriend(NMsnPersonRec Person)
{
    if (TreeCtrl)
        TreeCtrl->AddFriend(MsnPersonCopy(Person));
    MsnPersonFree(&Person);
}
NW_HOOK_METHOD(0x10155ce0, MsnWnd, NotifyAddFriend)

// 0x10155d90 (ret 0xC)
void MsnWnd::NotifyRemoveFriend(NMsnStringArg Account)
{
    if (TreeCtrl)
        TreeCtrl->RemoveFriend(MsnStringCopy(Account.Str()));
    Account.Str().~FString();
}
NW_HOOK_METHOD(0x10155d90, MsnWnd, NotifyRemoveFriend)

// 0x10155e40 (ret 0x1C)
void MsnWnd::NotifyAddGroup(NMsnGroupArg Group)
{
    if (TreeCtrl)
        TreeCtrl->AddGroup(MsnGroupCopy(Group));
    MsnGroupFree(&Group);
}
NW_HOOK_METHOD(0x10155e40, MsnWnd, NotifyAddGroup)

/*-----------------------------------------------------------------------------
    What the small windows and the buttons ask of the window (anonymous in the db; names ours).
-----------------------------------------------------------------------------*/

// 0x10155aa0 - the log-in window shown
void MsnWnd::ShowLoginWnd()
{
    LoginWindow->ShowWindow();
}
NW_HOOK_METHOD(0x10155aa0, MsnWnd, ShowLoginWnd)

// 0x10155ab0 - the version box shown
void MsnWnd::ShowVersionWnd()
{
    VersionWindow->ShowWindow();
}
NW_HOOK_METHOD(0x10155ab0, MsnWnd, ShowVersionWnd)

// 0x10155ac0 (ret 0x18) - the sign-in begins: the loading box shown, the log-in window and the message dialog hidden, and the
// protocol object signs in with the account and the password
void MsnWnd::StartLogin(NMsnStringArg Account, NMsnStringArg Password)
{
    LoadingWindow->ShowWithMode(1);
    LoginWindow->HideWindow();
    MessageDialog->HideWindow();
    MsnSignIn(MsnObj, MsnStringCopy(Account.Str()), MsnStringCopy(Password.Str()));    // the protocol object's SignIn
    Account.Str().~FString();
    Password.Str().~FString();
}
NW_HOOK_METHOD(0x10155ac0, MsnWnd, StartLogin)

// 0x10155b80 - the protocol object signs out (its sub-object at +0xC does)
void MsnWnd::SignOut()
{
    MsnObj->SignOut();                  // 0x1014f1c0
}
NW_HOOK_METHOD(0x10155b80, MsnWnd, SignOut)

// 0x10156300 - the options window shown and focused
void MsnWnd::ShowOptionWnd()
{
    OptionWindow->ShowWindow();
    OptionWindow->SetFocus();
}
NW_HOOK_METHOD(0x10156300, MsnWnd, ShowOptionWnd)

// 0x10155b90 (id 0xA52) - the status menu: hidden when shown, else shown and focused
INT MsnWnd::OnStatusButton()
{
    if (StatusWindow->IsShowWindow())
    {
        StatusWindow->HideWindow();
        return 1;
    }
    StatusWindow->ShowWindow();
    StatusWindow->SetFocus();
    return 1;
}
NW_HOOK_METHOD(0x10155b90, MsnWnd, OnStatusButton)

// 0x10158210 - signed in: every dialog but the "offline" one hidden (the added-person one stays), and that one shown
void MsnWnd::ShowOfflineDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        OfflineConfirmDialog->ShowWindow();
        OfflineConfirmDialog->SetFocus();
    }
}
NW_HOOK_METHOD(0x10158210, MsnWnd, ShowOfflineDialog)

// 0x101582a0 - the add-group dialog; its first control takes the focus
void MsnWnd::ShowAddGroupDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        AddGroupDialog->ShowWindow();
        ::nw::Field<NCWnd*>(AddGroupDialog, 0x1F8)->SetFocus();      // the dialog's edit line (AddGroupDialog.h EditBox, +0x1F8 in C4)
    }
}
NW_HOOK_METHOD(0x101582a0, MsnWnd, ShowAddGroupDialog)

// 0x10158340 - the rename-group dialog
void MsnWnd::ShowRenameGroupDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        RenameGroupDialog->ShowWindow();
        RenameGroupDialog->SetFocus();
    }
}
NW_HOOK_METHOD(0x10158340, MsnWnd, ShowRenameGroupDialog)

// 0x101583d0 - the remove-group dialog
void MsnWnd::ShowRemoveGroupDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        RemoveGroupDialog->ShowWindow();
        RemoveGroupDialog->SetFocus();
    }
}
NW_HOOK_METHOD(0x101583d0, MsnWnd, ShowRemoveGroupDialog)

// 0x10157f60 - the add-person dialog; its first control takes the focus
void MsnWnd::ShowAddFriendDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        AddFriendDialog->ShowWindow();
        ::nw::Field<NCWnd*>(AddFriendDialog, 0x1F8)->SetFocus();     // the dialog's edit line (AddFriendDialog.h EditBox, +0x1F8 in C4)
    }
}
NW_HOOK_METHOD(0x10157f60, MsnWnd, ShowAddFriendDialog)

// 0x10158000 - the remove-person dialog
void MsnWnd::ShowRemoveFriendDialog()
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        RemoveFriendDialog->ShowWindow();
        RemoveFriendDialog->SetFocus();
    }
}
NW_HOOK_METHOD(0x10158000, MsnWnd, ShowRemoveFriendDialog)

// 0x10158090 (ret 0x44) - the move-person dialog, given the person
void MsnWnd::ShowMoveGroupDialog(NMsnPersonRec Person)
{
    if (MsnLoggedIn(MsnObj))
    {
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        ((NMoveGroupDialogSetter*)MoveGroupDialog)->SetPerson(MsnPersonCopy(Person));      // the dialog keeps a copy of the person, 0x10156250
        MoveGroupDialog->ShowWindow();
        MoveGroupDialog->SetFocus();
    }
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x10158090, MsnWnd, ShowMoveGroupDialog, void (MsnWnd::*)(NMsnPersonRec))

// 0x10158460 (ret 4) - the message dialog with this text; the others (but the added-person one) hidden
void MsnWnd::ShowMessageDialog(const TCHAR* Text)
{
    AddFriendDialog->HideWindow();
    RemoveFriendDialog->HideWindow();
    MoveGroupDialog->HideWindow();
    OfflineConfirmDialog->HideWindow();
    AddGroupDialog->HideWindow();
    RenameGroupDialog->HideWindow();
    RemoveGroupDialog->HideWindow();
    ::nw::Field<FString>(MessageDialog, 0x1F4) = Text;
    MessageDialog->ShowWindow();
    MessageDialog->SetFocus();
}
NW_HOOK_METHOD(0x10158460, MsnWnd, ShowMessageDialog)

// 0x10158500 (id 0xA04) - signed in: the message when the messenger is blocked, else the add-person dialog
INT MsnWnd::OnAddFriendButton()
{
    if (MsnLoggedIn(MsnObj))
    {
        if (GMsnBlocked)
            ShowMessageDialog(*MSN_SYSMSG(SYSMSG_MsnBlocked));
        else
            ShowAddFriendDialog();
    }
    return 1;
}
NW_HOOK_METHOD(0x10158500, MsnWnd, OnAddFriendButton)

// the account of the entry chosen in the tree: its Text3, or NULL for none (repeated inline in the stock handlers)
static FORCEINLINE const TCHAR* SelectedAccount(MsnTreeCtrl* Ctrl)
{
    NCTVItem* Selected = Ctrl->MsnTreeWnd ? Ctrl->MsnTreeWnd->Selected : NULL;
    return Selected ? Selected->Data.Text3 : NULL;
}

// 0x10158550 (id 0xA05) - signed in: a person chosen in the tree gets the remove dialog, else the message "choose one"
INT MsnWnd::OnRemoveFriendButton()
{
    if (MsnLoggedIn(MsnObj))
    {
        const TCHAR* Message;
        if (GMsnBlocked)
        {
            Message = *MSN_SYSMSG(SYSMSG_MsnBlocked);
        }
        else
        {
            const TCHAR* Account = SelectedAccount(TreeCtrl);
            if (Account && *Account)
            {
                ShowRemoveFriendDialog();
                return 1;
            }
            Message = *MSN_SYSMSG(SYSMSG_NoFriendToRemove);
        }
        ShowMessageDialog(Message);
    }
    return 1;
}
NW_HOOK_METHOD(0x10158550, MsnWnd, OnRemoveFriendButton)

// 0x101585e0 (id 0xA06; name ours) - signed in: the person chosen is blocked, or unblocked when he is on the block list
// (the second list of the person lists); nobody chosen gives the message
INT MsnWnd::OnBlockButton()
{
    if (MsnLoggedIn(MsnObj))
    {
        const TCHAR* Message;
        if (GMsnBlocked)
        {
            Message = *MSN_SYSMSG(SYSMSG_MsnBlocked);
        }
        else
        {
            const TCHAR* Account = SelectedAccount(TreeCtrl);
            if (Account && *Account)
            {
                void* BlockList = MsnListBlocked(MsnGetLists(MsnObj));
                INT bBlock = 1;
                if (BlockList && MsnFindInList(BlockList, Account))
                    bBlock = 0;
                MsnBlock(MsnObj, Account, bBlock);                       // the protocol object's Block
                return 1;
            }
            Message = *MSN_SYSMSG(SYSMSG_NoFriendToBlock);
        }
        ShowMessageDialog(Message);
    }
    return 1;
}
NW_HOOK_METHOD(0x101585e0, MsnWnd, OnBlockButton)

// 0x101586c0 (id 0xA07) - signed in: the person chosen is looked up in the person list (the first list) and given to the
// move-group dialog; nobody chosen gives the message
INT MsnWnd::OnMoveGroupButton()
{
    if (MsnLoggedIn(MsnObj))
    {
        const TCHAR* Message;
        if (GMsnBlocked)
        {
            Message = *MSN_SYSMSG(SYSMSG_MsnBlocked);
        }
        else
        {
            const TCHAR* Account = SelectedAccount(TreeCtrl);
            if (Account && *Account)
            {
                void* FriendList = MsnListFriends(MsnGetLists(MsnObj));
                if (FriendList)
                {
                    NMsnPersonRec* Found = (NMsnPersonRec*)MsnFindInList(FriendList, Account);
                    if (Found)
                        ShowMoveGroupDialog(MsnPersonCopy(*Found));
                }
                return 1;
            }
            Message = *MSN_SYSMSG(SYSMSG_NoFriendToMove);
        }
        ShowMessageDialog(Message);
    }
    return 1;
}
NW_HOOK_METHOD(0x101586c0, MsnWnd, OnMoveGroupButton)

// 0x1015f400 (id 0xA08) - the sign-out: the 5 s timer starts, everything is hidden but the loading box (shown for the
// sign-out), the chat windows are closed one by one (the first of the map each time) and the protocol object signs out
INT MsnWnd::OnSignOutButton()
{
    SignOutTimer = SetTimer(MSN_SignOutTimerID, MSN_SignOutDelay, NULL);
    LoginWindow->HideWindow();
    VersionWindow->HideWindow();
    TreeCtrl->HideWindow();
    if (L2FriendWindow)
        L2FriendWindow->HideWindow();
    OptionWindow->HideWindow();
    AlarmWindow->HideWindow();
    StatusWindow->HideWindow();
    AddFriendDialog->HideWindow();
    RemoveFriendDialog->HideWindow();
    MoveGroupDialog->HideWindow();
    AddedConfirmDialog->HideWindow();
    OfflineConfirmDialog->HideWindow();
    AddGroupDialog->HideWindow();
    RenameGroupDialog->HideWindow();
    RemoveGroupDialog->HideWindow();
    MessageDialog->HideWindow();
    StatusButton->HideWindow();
    AddFriendButton->HideWindow();
    RemoveFriendButton->HideWindow();
    MoveGroupButton->HideWindow();
    AddGroupButton->HideWindow();
    SignOutButton->HideWindow();
    LoadingWindow->ShowWithMode(0);
    INT Count = ChatWnds.Num;
    if (Count > 0)
    {
        do
        {
            NCWnd* Chat = ChatWnds.Data[0].Wnd;
            if (Chat)
            {
                Chat->HideWindow();
                OnChatClosed(Chat);                                               // the chat window leaves the maps, 0x1015d380
            }
            Count--;
        } while (Count != 0);
    }
    SignOut();
    return 1;
}
NW_HOOK_METHOD(0x1015f400, MsnWnd, OnSignOutButton)

// 0x1015d430 (ret 8) - a double click in the tree: on a person that is not offline the chat opens
INT MsnWnd::OnDoubleClick(INT X, INT Y)
{
    NCTVItem* Hit = TreeCtrl->MsnTreeWnd->FindItemAt(X, Y, TreeCtrl->MsnTreeWnd->Root);
    if (Hit)
    {
        NTVItemData Item = Hit->Data;
        if (Item.Unknown2848 == 3)
        {
            NMsnPersonRec* Friend = MsnFindFriend(MsnObj, Item.Text3);
            if (Friend->Status() != TEXT("FLN"))
                OpenChat(MsnPersonCopy(*Friend));
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1015d430, MsnWnd, OnDoubleClick)

// 0x1015ce90 (ret 0x44) - the chat with this person: the task window is shown (0x10135F10); the chat window open already for the
// account is shown and focused (and its entry in the task window marked free); else a window of the table that is free, or a new
// ChatWnd (256 x 401 at a random place of the desktop, made with the owner, the protocol object, the account and a copy of the
// signed-in person), is entered by the account, titled with the person's name, given the account, shown and focused, and the
// protocol object is told to open the conversation. The record is freed here
void MsnWnd::OpenChat(NMsnPersonRec Person)
{
    ((NMsnWndOps*)this)->ShowTaskWnd();                                                       // 0x1015AA90
    FString Account(Person.Field20());
    NCWnd* Chat = ((NMsnWndOps*)this)->FindOneToOneChat(Account);                             // the chat window open for the account, 0x1015B820
    if (!Chat)
    {
        Chat = ((NMsnWndOps*)this)->FindFreeChat();                                           // else one that is free, 0x1015B920
        if (!Chat)
        {
#if NW_HOOK_MODE
            Chat = NewStockObj<NCWnd>(NClass::ChatWnd(), 0x10143BA0, this, MsnObj, MsnStringCopy(Account), MsnPersonCopy(Me));
#else
            Chat = new ChatWnd(this, MsnObj, Account, *(const NChatUser*)&Me);                // ChatWnd's second constructor, 0x10143BA0
#endif
            INT X = appRand() % 200 + 200;
            INT Y = appRand() % 150 + 150;
            Chat->SetPosition(X, Y, 0x100, 0x191);
            GConsole->Desktop->CreateChild(Chat, 0x2002);
        }
        ((NMsnStrMapOps*)&ChatWnds)->Add(*Account, (void**)&Chat);                         // entered by the account, 0x10151560
        {
            NMsnRawFString Shown;
            FString& Name = *Person.GetDisplayName(Shown.Get());
            Chat->SetTitle(*Name);
            Name.~FString();
        }
        ((NMsnChatOps*)Chat)->SetPeerAccount(Account);                                     // the chat window keeps the account, 0x101561F0
        static_cast<ChatWnd*>(Chat)->Unknown8F8 = 1;                                       // (ChatWnd +0x918)
        Chat->ShowWindow();
        Chat->SetFocus();
        ((NChatMsnRequester*)MsnObj)->RequestSession(Account);                             // the protocol object opens the conversation, 0x1014F220
    }
    else
    {
        {
            NMsnRawFString Shown;
            FString& Name = *Person.GetDisplayName(Shown.Get());
            Chat->SetTitle(*Name);
            Name.~FString();
        }
        ((NMsnChatOps*)Chat)->SetPeerAccount(Account);                                     // 0x101561F0
        static_cast<ChatWnd*>(Chat)->Unknown8F8 = 1;                                       // (ChatWnd +0x918)
        Chat->ShowWindow();
        Chat->SetFocus();
        if (L2FriendWindow)
        {
            NCWnd* Listed = ((NMsnTaskWndOps*)L2FriendWindow)->FindTaskOfSession(static_cast<ChatWnd*>(Chat)->Switchboard);   // its entry in the task window, 0x10141C30
            if (Listed)
                ::nw::Field<INT>(Listed, 0xF0) = 0;               // (C4: the task entry's +0xF0, written 0 by the stock at 0x1015D138)
        }
    }
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x1015ce90, MsnWnd, OpenChat, void (MsnWnd::*)(NMsnPersonRec))

// 0x1015e070 (ret 0x44) - the alarm box's click: the chat with a copy of the person; the person is freed
void MsnWnd::OnAlarmClick(NMsnPersonRec Person)
{
    OpenChat(MsnPersonCopy(Person));
    MsnPersonFree(&Person);
}
NW_HOOK_METHOD(0x1015e070, MsnWnd, OnAlarmClick)

/*-----------------------------------------------------------------------------
    What the protocol object tells the window.
-----------------------------------------------------------------------------*/

// 0x10155c60 (ret 0xC) - a new mail: the alarm box shows the subject, when the user wants it
void MsnWnd::OnNewMail(NMsnStringArg Subject)
{
    if (GMsnNewEMailAlarm)
    {
        AlarmWindow->SetMail(MsnStringCopy(Subject.Str()));
        AlarmWindow->ShowWindow();
    }
    Subject.Str().~FString();
}
NW_HOOK_VARARGS(0x10155c60, MsnWnd, OnNewMail, void (MsnWnd::*)(NMsnStringArg))

// 0x10155ef0 (ret 0x44) - the sign-in is done: the log-in, version and loading pages go, the tree and the six buttons show, and the
// person given is the one signed in
void MsnWnd::OnSignedIn(NMsnPersonRec Self)
{
    LoginWindow->HideWindow();
    VersionWindow->HideWindow();
    LoadingWindow->HideWindow();
    TreeCtrl->ShowWindow();
    StatusButton->ShowWindow();
    AddFriendButton->ShowWindow();
    RemoveFriendButton->ShowWindow();
    MoveGroupButton->ShowWindow();
    AddGroupButton->ShowWindow();
    SignOutButton->ShowWindow();
    Me.Assign(&Self);
    MsnPersonFree(&Self);
}
NW_HOOK_VARARGS(0x10155ef0, MsnWnd, OnSignedIn, void (MsnWnd::*)(NMsnPersonRec))

// 0x10155fd0 (ret 0x44) - the person signed in is a new one
void MsnWnd::SetMe(NMsnPersonRec Self)
{
    Me.Assign(&Self);
    MsnPersonFree(&Self);
}
NW_HOOK_VARARGS(0x10155fd0, MsnWnd, SetMe, void (MsnWnd::*)(NMsnPersonRec))

// 0x10158180 (ret 0x44) - signed in: the box that tells that this person added us (and asks whether to add him) is filled, shown and
// focused
void MsnWnd::ShowAddedConfirm(NMsnPersonRec Person)
{
    if (MsnLoggedIn(MsnObj))
    {
        MsnAddedConfirmAddBuddy(AddedConfirmDialog, Person);                           // AddedConfirmDialog::AddBuddy 0x101473d0: it queues a copy
        AddedConfirmDialog->ShowWindow();
        AddedConfirmDialog->SetFocus();
    }
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x10158180, MsnWnd, ShowAddedConfirm, void (MsnWnd::*)(NMsnPersonRec))

// 0x10158790 (ret 0x44) - the group of this person in the tree is emptied and filled again from the person list
void MsnWnd::RefreshGroupOf(NMsnPersonRec Person)
{
    NCTVItem* Group = (NCTVItem*)((NMsnTreeCtrlView*)TreeCtrl)->FindGroupNode(Person.GroupID());     // the group entry with that id, 0x10166a60
    if (Group)
        TreeCtrl->RefreshGroup(&Group);
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x10158790, MsnWnd, RefreshGroupOf, void (MsnWnd::*)(NMsnPersonRec))

// 0x10158800 (ret 0x44) - somebody added us: unless the user does not want to be disturbed, and while signed in, the box asks
void MsnWnd::OnFriendRequest(NMsnPersonRec Person)
{
    if (!GMsnDontDisturb && MsnLoggedIn(MsnObj))
        ShowAddedConfirm(MsnPersonCopy(Person));
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x10158800, MsnWnd, OnFriendRequest, void (MsnWnd::*)(NMsnPersonRec))

// The chat windows are told of a person's change (repeated inline in the stock code): each one open asks whether it is with this
// account and, when it is, is told
static FORCEINLINE void TellChatWnds(NMsnWndStrMap& ChatWnds, FString& Account)
{
    for (INT i = 0; i < ChatWnds.Num; i++)
    {
        NCWnd* Chat = ChatWnds.Data[i].Wnd;
        if (Chat)
        {
            if (static_cast<ChatWnd*>(Chat)->HasAccount(Account))        // 0x101417B0
                static_cast<ChatWnd*>(Chat)->RefreshRoster();              // 0x10141880
        }
    }
}

// 0x1015bb10 (name ours; ret 0x44) - a person's record changed: his entry is looked up before and his group filled again; when he
// had no entry or his record says "FLN" the login alarm shows (if wanted); the chat windows are told
void MsnWnd::OnFriendPresence(NMsnPersonRec Person)
{
    NCTVItem* Group = NULL;
    MsnTree* Tree = TreeCtrl->MsnTreeWnd;
    NCTVItem* Old = TreeCtrl->FindFriendItem(*Person.Field20(), Tree ? Tree->Root : NULL);
    Group = (NCTVItem*)((NMsnTreeCtrlView*)TreeCtrl)->FindGroupNode(Person.GroupID());       // 0x10166A60
    if (Group)
        TreeCtrl->RefreshGroup(&Group);
    if (!Old || Person.Name() == TEXT("FLN"))
    {
        if (GMsnLoginAlarm)
        {
            AlarmWindow->SetOnline(MsnPersonCopy(Person));
            AlarmWindow->ShowWindow();
        }
    }
    TellChatWnds(ChatWnds, Person.Field20());
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x1015bb10, MsnWnd, OnFriendPresence, void (MsnWnd::*)(NMsnPersonRec))

// 0x1015bc80 (name ours; ret 0x44) - the same without the entry lookup and the alarm
void MsnWnd::RefreshFriend(NMsnPersonRec Person)
{
    NCTVItem* Group = (NCTVItem*)((NMsnTreeCtrlView*)TreeCtrl)->FindGroupNode(Person.GroupID());       // 0x10166A60
    if (Group)
        TreeCtrl->RefreshGroup(&Group);
    TellChatWnds(ChatWnds, Person.Field20());
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x1015bc80, MsnWnd, RefreshFriend, void (MsnWnd::*)(NMsnPersonRec))

// 0x1015cd90 (name ours; ret 4) - the chat window of the account the session talks to is looked up (0x10136E30: its answer is not
// used here)
void MsnWnd::ShowChatOfSession(void* Session)
{
    if (Session)
    {
        void* Peer = ((SwitchboardSession*)Session)->GetLastFriend();      // 0x10152bf0
        if (Peer)
            ((NMsnWndOps*)this)->FindOneToOneChat(FString(*::nw::Field<FString>(Peer, 0x20)));     // 0x1015b820 (the answer is not used)
    }
}
NW_HOOK_METHOD(0x1015cd90, MsnWnd, ShowChatOfSession)

// 0x1015d380 (name ours; ret 4) - a chat window closed: the session it talked through (ChatWnd +0xF0) has its account's entry taken out of the
// table of names (0x10151E60) and is closed by the protocol object (0x1014F210); the window's own entry (the account it is about,
// ChatWnd +0x958) leaves the chat table, the task window (the field at +0x180, a TaskWnd: its RemoveTask 0x1014A030 is the code of
// L2TaskWnd's too) forgets it and hides when its map is empty, the chat window is reset (0x10142EC0) and entered in the number table
void MsnWnd::OnChatClosed(NCWnd* Chat)
{
    ChatWnd* ChatW = static_cast<ChatWnd*>(Chat);
    void* Session = ChatW->Switchboard;
    if (Session)
    {
        FString* Account = ((SwitchboardSession*)Session)->GetSessionIDRef();             // the session's account (its +0x64), 0x10152B90
        ((NMsnStrMapOps*)&ChatNames)->RemoveKey(**Account);                               // 0x10151E60
        ((NChatMsnRequester*)MsnObj)->CloseSession((SwitchboardSession*)Session);         // 0x1014F210
    }
    ChatWnds.RemoveWnd(*ChatW->Unknown938, &Chat);
    if (L2FriendWindow)
        ((L2TaskWnd*)L2FriendWindow)->RemoveTask(Chat);                // TaskWnd's RemoveTask is L2TaskWnd's code, 0x1014A030
    if (L2FriendWindow && static_cast<TaskWnd*>(L2FriendWindow)->Tasks.Pairs.Num() == 0)
        L2FriendWindow->HideWindow();
    ChatW->ResetSession();                                             // 0x10142EC0
    INT Key = (INT)Chat;
    InviteWnds.SetWnd(&Key, &Chat);
}
NW_HOOK_METHOD(0x1015d380, MsnWnd, OnChatClosed)

// 0x1015ca80 (name ours) - the sign-out timer stops; every chat window is told to forget its conversation, is entered in the number
// table (by its own address) and hidden; the two name tables are emptied, the friends page (the chat group) is put away, the
// person lists of the protocol object are cleared and the tree is emptied
void MsnWnd::ResetAfterSignOut()
{
    if (SignOutTimer)
    {
        KillTimer(SignOutTimer);
        SignOutTimer = 0;
    }
    for (INT i = 0; i < ChatWnds.Num; i++)
    {
        NCWnd* Chat = ChatWnds.Data[i].Wnd;
        if (Chat)
        {
            static_cast<ChatWnd*>(Chat)->ResetSession();                   // 0x10142EC0
            INT Key = (INT)Chat;
            InviteWnds.SetWnd(&Key, &Chat);                                // 0x10151770
            Chat->HideWindow();
        }
    }
    ChatWnds.Reset();
    ChatNames.Reset();
    if (L2FriendWindow)
    {
        ((NChatTaskWnd*)L2FriendWindow)->DestroyTasks();               // 0x10141C90
        L2FriendWindow->HideWindow();
    }
    ((NMsnContacts*)MsnGetLists(MsnObj))->DestroyLists();              // 0x10152150
    TreeCtrl->Clear();
}
NW_HOOK_METHOD(0x1015ca80, MsnWnd, ResetAfterSignOut)

// 0x1015d180 (ret 0xC) - the protocol object signed out for this reason: everything is put away; when the window is minimized its
// icon shows "offline", else the window goes back to the log-in page; the reasons "OTH" (signed in somewhere else) and "SSD" (the
// server is going down) raise the alarm of type 3 and 4
void MsnWnd::OnSignedOut(NMsnStringArg Reason)
{
    ResetAfterSignOut();
    MessengerWnd* Holder = (MessengerWnd*)Entry->Parent->Wnd;
    if (Holder->IsIconActive())
    {
        Holder->SetMinimizedIcon(TEXT("L2UI_CH3.Msn.msnminiicon5"));
    }
    else if (Holder->IsShowWindow())
    {
        VersionWindow->HideWindow();
        LoadingWindow->HideWindow();
        TreeCtrl->HideWindow();
        if (L2FriendWindow)
            L2FriendWindow->HideWindow();
        AddFriendDialog->HideWindow();
        RemoveFriendDialog->HideWindow();
        MoveGroupDialog->HideWindow();
        OfflineConfirmDialog->HideWindow();
        AddGroupDialog->HideWindow();
        RenameGroupDialog->HideWindow();
        RemoveGroupDialog->HideWindow();
        MessageDialog->HideWindow();
        AlarmWindow->HideWindow();
        StatusButton->HideWindow();
        AddFriendButton->HideWindow();
        RemoveFriendButton->HideWindow();
        MoveGroupButton->HideWindow();
        AddGroupButton->HideWindow();
        SignOutButton->HideWindow();
        LoginWindow->ShowWindow();
    }
    if (Reason.Str() == TEXT("OTH"))
    {
        AlarmWindow->AlarmType = 3;
        AlarmWindow->ShowWindow();
    }
    else if (Reason.Str() == TEXT("SSD"))
    {
        AlarmWindow->AlarmType = 4;
        AlarmWindow->ShowWindow();
    }
    Reason.Str().~FString();
}
NW_HOOK_VARARGS(0x1015d180, MsnWnd, OnSignedOut, void (MsnWnd::*)(NMsnStringArg))

// 0x10158880 (ret 0xC; was anonymous) - the server's error number as a dialog: 201 and 205 say SystemMsg 1341, 207 says 1342; 911 and 930
// say 1343 and 1344 and put the log-in page back (everything else hidden, the log-in window shown). The code is destroyed on return
void MsnWnd::ShowError(FString Code)
{
    if (Code == TEXT("201") || Code == TEXT("205"))
        ShowMessageDialog(*MSN_SYSMSG(1341));
    else if (Code == TEXT("207"))
        ShowMessageDialog(*MSN_SYSMSG(1342));
    else
    {
        FString* Text = NULL;
        if (Code == TEXT("911"))
            Text = &MSN_SYSMSG(1343);
        else if (Code == TEXT("930"))
            Text = &MSN_SYSMSG(1344);
        if (Text)
        {
            ShowMessageDialog(**Text);
            VersionWindow->HideWindow();
            LoadingWindow->HideWindow();
            TreeCtrl->HideWindow();
            if (L2FriendWindow)
                L2FriendWindow->HideWindow();
            StatusButton->HideWindow();
            AddFriendButton->HideWindow();
            RemoveFriendButton->HideWindow();
            MoveGroupButton->HideWindow();
            AddGroupButton->HideWindow();
            SignOutButton->HideWindow();
            LoginWindow->ShowWindow();
        }
    }
}
typedef void (MsnWnd::*SigMsnShowError)(FString);
NW_HOOK_VARARGS(0x10158880, MsnWnd, ShowError, SigMsnShowError)

// 0x1015cdd0 (ret 4; was anonymous) - a session left the manager: the chat window kept under the session's id (the second table) forgets it
// (its own session and its invite window's), and the id leaves that table
void MsnWnd::OnSessionRemoved(SwitchboardSession* Session)
{
    if (Session)
    {
        NCWnd* Wnd;
        {
            FString Key(**Session->GetSessionIDRef());
            Wnd = ChatNames.FindWnd(&Key);
        }
        if (Wnd)
        {
            ChatWnd* Chat = static_cast<ChatWnd*>(Wnd);
            Chat->Switchboard = NULL;
            Chat->InviteWindow->Switchboard = NULL;
        }
        ((NMsnStrMapOps*)&ChatNames)->RemoveKey(**Session->GetSessionIDRef());     // 0x10151e60
    }
}
NW_HOOK_METHOD(0x1015cdd0, MsnWnd, OnSessionRemoved)

// 0x10156250 (ret 0x44, name ours) - MoveGroupDialog: the person it will move (its record at +0x1D8) is the argument's (the argument is
// destroyed on return)
void NMoveGroupDialogSetter::SetPerson(NMsnPersonRec Person)
{
    ::nw::Field<NMsnPersonRec>(this, 0x1F8).Assign(&Person);
    MsnPersonFree(&Person);
}
NW_HOOK_VARARGS(0x10156250, NMoveGroupDialogSetter, SetPerson, void (NMoveGroupDialogSetter::*)(NMsnPersonRec))

NW_CHECK_VTABLE(MsnWnd, 0x101dd2d8, 74)
