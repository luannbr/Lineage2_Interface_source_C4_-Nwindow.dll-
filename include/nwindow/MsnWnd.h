#pragma once
// MsnWnd - the messenger's main window (UClass "CMsnWnd", 0x1B0; C3 only, no C2 source: layout and virtuals from the
// stock constructor 0x1013AE40, OnCreate 0x10139A40, OnDestroy 0x10137A60, ShowWindow 0x10136740 and the handlers
// below). It owns the protocol object (Msn, 0x58 bytes: another lane's), the chat windows, the person tree and the
// small windows of the messenger (log in, version, loading, options, alarm, status, the dialogs). The tree, the
// status button, the five lower buttons and the "sign out" button report to it with NWM_COMMAND.
//
// The three maps are the engine's TMap (TMapBase: the pairs TArray, the hash table, its size) of which only the
// layout is declared (the stock code rehashes through 0x1012CA90 / 0x100626E0 and destroys them out of line).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class Msn;
class SwitchboardSession;

// The game data's strings by their number, as the stock code reads them: one add of a constant. C4: the system strings are at
// GL2GameData + 0x9871C + 12 * N and the text of a system message at + 0x722C8 + 0x30 * N + 4 (C3: 0x65A8C and 0x3F638); the typed
// tables of NGameData.h / UE2Engine.h give those (read off MsnWnd::OnAddFriendButton 0x10158500: add ecx, 0x8183C for N = 1309)
#define MSN_SYSSTR(N) (GL2GameData.SysString[N])
#define MSN_SYSMSG(N) (GL2GameData.SystemMsg[N].Message)
class NCButton;
class LoginWnd;
class VersionWnd;
class LoadingWnd;
class MsnTreeCtrl;
class OptionWnd;
class AlarmWnd;
class StatusWnd;
class NCTVItem;

// The messenger's person record (0x44 bytes, name ours): two words and five FStrings. The stock code builds, copies and
// frees it out of line (0x1011A140 / 0x1012C460 / 0x1012C530, the protocol lane's). NMsnPersonRec is the plain record, what a
// function takes BY VALUE (the stock callers make the copy and the callee frees it, so the C++ side must not: see
// MsnPersonCopy / MsnPersonFree); NMsnPerson is the one a member holds (built and freed with its owner).
struct NMsnPersonRec
{
    BYTE Raw[0x44];
    // +0x00 (not touched by the constructor), +0x04 copied, +0x08 / +0x14 / +0x20 / +0x2C / +0x38 the FStrings
    FString& Name() { return *(FString*)(Raw + 0x08); }              // the account, going by the add-friend dialog (name ours)
    FString& Status() { return *(FString*)(Raw + 0x14); }            // the presence code ("NLN", "FLN"...) (name ours)
    FString& Field20() { return *(FString*)(Raw + 0x20); }           // the account the tree keeps in Text3 (name ours)
    INT& GroupID() { return *(INT*)(Raw + 0x04); }                   // the id of the group it belongs to (name ours)
    // 0x101510D0 (ret 4): the name shown, built from +0x2C / +0x38 into Out, which must not be a built FString (name ours).
    // Out of line (MsnWnd.cpp): NMsnFriend::GetDisplayName, the same record in the protocol lane's header
    FString* GetDisplayName(FString* Out);
    // 0x10151070 (ret 4): every string and the group id copied from Src (name ours). Out of line (MsnWnd.cpp): NMsnFriend::operator=
    void Assign(const NMsnPersonRec* Src);
};
NW_ASSERT_SIZE(NMsnPersonRec, 0x44);

// 0x10150F20 (ret 4): a copy of a record, for a callee that will free it (the record is returned in the argument slot).
// Out of line (MsnWnd.cpp): the copy constructor of NMsnFriend
NMsnPersonRec MsnPersonCopy(const NMsnPersonRec& Src);
// 0x10150FF0: the strings of a record freed (what a callee does with its by-value argument). Out of line (MsnWnd.cpp): ~NMsnFriend
void MsnPersonFree(NMsnPersonRec* Rec);

// The record a member holds: built and freed with its owner. Out of line (MsnWnd.cpp): 0x1013E9A0 (NChatUser::NChatUser) and ~NMsnFriend
// (0x10150FF0)
struct NMsnPerson : public NMsnPersonRec
{
    NMsnPerson();
    ~NMsnPerson();
};
NW_ASSERT_SIZE(NMsnPerson, 0x44);

// The group record the tree is given (0x1C bytes, name ours): two FStrings (+0x00, +0x0C) and a number (+0x18). Passed by
// value like the person record (copied by the caller, freed by the callee: 0x1012DB00 / 0x1012DB80)
struct NMsnGroupArg
{
    BYTE Raw[0x1C];
    FString& Name() { return *(FString*)Raw; }                       // the group's name (name ours)
    INT& ID() { return *(INT*)(Raw + 0x18); }                        // its id (name ours)
};
NW_ASSERT_SIZE(NMsnGroupArg, 0x1C);

// Out of line (MsnWnd.cpp): the copy constructor 0x101526E0 and the destructor 0x10152760 of NMsnGroup (the protocol lane's header)
NMsnGroupArg MsnGroupCopy(const NMsnGroupArg& Src);
void MsnGroupFree(NMsnGroupArg* Rec);

// A string passed by value to a function that frees it (the callee owns the copy): the 12 bytes of an FString as a plain blob,
// so the C++ side adds no copy of its own (name ours)
struct NMsnStringArg
{
    BYTE Raw[sizeof(FString)];
    FString& Str() { return *(FString*)Raw; }
};
FORCEINLINE NMsnStringArg MsnStringCopy(const FString& Src)
{
    NMsnStringArg Copy;
    new ((void*)Copy.Raw) FString(Src);
    return Copy;
}

FORCEINLINE NMsnStringArg MsnStringArgOf(const TCHAR* Text)
{
    NMsnStringArg Arg;
    new ((void*)Arg.Raw) FString(Text);
    return Arg;
}

// A pair of a map whose key is a string: the next pair of the hash chain, the key and the value (0x14 bytes)
struct NMsnWndStrPair
{
    INT HashNext;                    // +0x00
    FString Key;                     // +0x04
    NCWnd* Wnd;                      // +0x10 (map +0xCC: the chat window; map +0xE0: a word, not read here)
};
NW_ASSERT_SIZE(NMsnWndStrPair, 0x14);

// A pair of the map whose key is a number (0xC bytes)
struct NMsnWndIntPair
{
    INT HashNext;                    // +0x00
    INT Key;                         // +0x04
    NCWnd* Wnd;                      // +0x08
};
NW_ASSERT_SIZE(NMsnWndIntPair, 0xC);

// The layout of TMapBase (the stock constructor sets the hash size 8): Pairs, the hash table, its size. The constructor's
// rehash and the destructor are the functions of the derived table D (D::Rehash, D::Free: one function each, out of line, as in the
// stock code); Pairs() is the same memory seen as the TArray it starts with
template <class P, class D> struct NMsnWndMap
{
    P* Data;                         // +0x00 Pairs
    INT Num;                         // +0x04
    INT Max;                         // +0x08
    INT* Hash;                       // +0x0C the table of chain heads (-1 empty), HashCount entries
    INT HashCount;                   // +0x10 8, doubled when the pairs outgrow it

    NMsnWndMap() : Data(NULL), Num(0), Max(0), Hash(NULL), HashCount(8) { static_cast<D*>(this)->Rehash(); }
    ~NMsnWndMap() { static_cast<D*>(this)->Free(); }
    TArray<P>& Pairs() { return *(TArray<P>*)this; }
    // everything out (the stock code repeats it inline): the pairs destroyed, the array emptied, the table rebuilt
    FORCEINLINE void Reset()
    {
        for (INT i = 0; i < Num; i++)
            Data[i].~P();
        ((FArray*)this)->Empty(sizeof(P));
        HashCount = 8;
        static_cast<D*>(this)->Rehash();
    }
};

// The table by name (the chat windows) and the table by number, with the stock functions that look up and set their windows.
// Rehash is the map's own rehash (NMsnStrMap::Rehash 0x1013FC50 of FriendList.cpp / NMsnIntMap::Rehash 0x10089D40 of MSNSocket.cpp: the
// same layout), a one-line forwarder here; Free is the destructor's body (0x1015E0D0 / 0x1015BF20)
struct NMsnWndStrMap : public NMsnWndMap<NMsnWndStrPair, NMsnWndStrMap>
{
    __declspec(noinline) void Rehash();                                           // 0x1013FC50 (FriendList.cpp)
    __declspec(noinline) NCWnd* FindWnd(const FString* Key);                      // 0x101347D0 (ret 4) the window entered by this name, NULL for none
    __declspec(noinline) NCWnd** SetWnd(const TCHAR* Key, NCWnd** Value);         // 0x101361B0 (ret 8) the window entered by this name is Value; its place
    __declspec(noinline) INT RemoveWnd(const TCHAR* Key, NCWnd** Value);          // 0x10137450 (ret 8) the entries of this name with this window go; how many
    __declspec(noinline) void FreePairs();                                        // 0x10137560 the pairs removed (their names freed) and the array freed
    __declspec(noinline) void Free();                                             // 0x10139760 the hash freed, then the pairs (the destructor's body)
};
struct NMsnWndIntMap : public NMsnWndMap<NMsnWndIntPair, NMsnWndIntMap>
{
    __declspec(noinline) void Rehash();                                           // 0x10089D40 (MSNSocket.cpp)
    __declspec(noinline) NCWnd** SetWnd(const INT* Key, NCWnd** Value);           // 0x10136230 (ret 8) the same for a number
    __declspec(noinline) void FreePairs();                                        // 0x101364A0 the pairs removed and the array freed
    __declspec(noinline) void Free();                                             // 0x101375B0 the hash freed, then the pairs (the destructor's body)
};

class MsnWnd : public NCWnd
{
    NW_DECLARE_CLASS(MsnWnd, NCWnd, 0, 0x103250D0)

    Msn* MsnObj;                         // +0xE8 the protocol object, 0x58 bytes (made by OnCreate with 0x1012C180)
    NMsnWndStrMap ChatWnds;                 // +0xEC the open chat windows, by the other person's account (name ours)
    NMsnWndStrMap ChatNames;                // +0x100 a second map by account (name ours; its values are not read here)
    NMsnWndIntMap InviteWnds;               // +0x114 windows by a number (name ours): ShowWindow hides them, OnTimer fills it
    NMsnPerson Me;                       // +0x128 the signed-in person (name ours)
    INT SignOutTimer;                    // +0x16C the timer 0xA55 started by the "sign out" button (name ours)
    LoginWnd* LoginWindow;               // +0x170 shown until the sign-in is done
    VersionWnd* VersionWindow;           // +0x174
    LoadingWnd* LoadingWindow;           // +0x178
    MsnTreeCtrl* TreeCtrl;               // +0x17C the persons and groups
    NCWnd* L2FriendWindow;               // +0x180 the Lineage II friends (L2FriendWnd), made by the stock chat code
    OptionWnd* OptionWindow;             // +0x184 child of the desktop, not of this window
    AlarmWnd* AlarmWindow;               // +0x188 child of the desktop
    StatusWnd* StatusWindow;             // +0x18C
    NCWnd* AddFriendDialog;              // +0x190 the dialogs are children of the desktop (NConsoleWnd::Desktop)
    NCWnd* RemoveFriendDialog;           // +0x194
    NCWnd* MoveGroupDialog;              // +0x198
    NCWnd* AddedConfirmDialog;           // +0x19C
    NCWnd* OfflineConfirmDialog;         // +0x1A0
    NCWnd* AddGroupDialog;               // +0x1A4
    NCWnd* RenameGroupDialog;            // +0x1A8
    NCWnd* RemoveGroupDialog;            // +0x1AC
    NCWnd* MessageDialog;                // +0x1B0
    NCButton* StatusButton;              // +0x1B4 id 0xA52, the little arrow by the status
    NCButton* AddFriendButton;           // +0x1B8 id 0xA04
    NCButton* RemoveFriendButton;        // +0x1BC id 0xA05
    NCButton* MoveGroupButton;           // +0x1C0 id 0xA06
    NCButton* AddGroupButton;            // +0x1C4 id 0xA07
    NCButton* SignOutButton;             // +0x1C8 id 0xA08
    UTexture* BackTex;                   // +0x1CC "L2UI_CH3.Msn.msn_back0"

    MsnWnd();                                                        // 0x1015f820
    // ~MsnWnd                                                       // 0x1015f9c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1015f710
    virtual INT OnTimer(INT TimerID);                                // 36 0x1015c8c0
    virtual INT OnCreate();                                          // 37 0x1015e3b0
    virtual INT OnDestroy();                                         // 38 0x1015c3d0
    virtual void ShowWindow();                                       // 48 0x1015b130
    virtual void HideWindow();                                       // 49 0x1015b550
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10157870

    // notifications from the protocol object
    void NotifyAddFriend(NMsnPersonRec Person);                         // 0x10155ce0 (ret 0x44)
    void NotifyRemoveFriend(NMsnStringArg Account);                  // 0x10155d90 (ret 0xC)
    void NotifyAddGroup(NMsnGroupArg Group);                            // 0x10155e40 (ret 0x1C)

    // names ours (anonymous in the db): what the buttons and the windows of the messenger ask of it
    void ShowLoginWnd();                                             // 0x10155aa0 (tail call of the window's ShowWindow)
    void ShowVersionWnd();                                           // 0x10155ab0 the same for the version box
    void StartLogin(NMsnStringArg Account, NMsnStringArg Password);  // 0x10155ac0 (ret 0x18) the sign-in begins
    __declspec(noinline) void SignOut();                             // 0x10155b80 (tail call into the protocol object)
    void ShowOptionWnd();                                            // 0x10156300 the options window shown and focused
    void ShowOfflineDialog();                                        // 0x10158210 the "appear offline" dialog
    void ShowAddGroupDialog();                                       // 0x101582a0
    void ShowRenameGroupDialog();                                    // 0x10158340
    void ShowRemoveGroupDialog();                                    // 0x101583d0
    void ShowAddFriendDialog();                                      // 0x10157f60
    void ShowRemoveFriendDialog();                                   // 0x10158000
    void ShowMoveGroupDialog(NMsnPersonRec Person);                  // 0x10158090 (ret 0x44; hooked by NW_HOOK_VARARGS: the method itself,
                                                                     //   not NW_HOOK_METHOD's forwarder that copies the record once more)
    void ShowMessageDialog(const TCHAR* Text);                       // 0x10158460 (ret 4)
    void OpenChat(NMsnPersonRec Person);                                // 0x1015ce90 (ret 0x44; the chat lane's)
    void OnAlarmClick(NMsnPersonRec Person);                            // 0x1015e070 (ret 0x44) the alarm window's click opens the chat
    INT OnStatusButton();                                            // 0x10155b90 (id 0xA52) toggles the status menu
    INT OnAddFriendButton();                                         // 0x10158500 (id 0xA04)
    INT OnRemoveFriendButton();                                      // 0x10158550 (id 0xA05)
    INT OnBlockButton();                                             // 0x101585e0 (id 0xA06) (name ours) blocks / unblocks the selected person
    INT OnMoveGroupButton();                                         // 0x101586c0 (id 0xA07)
    INT OnSignOutButton();                                           // 0x1015f400 (id 0xA08)
    INT OnDoubleClick(INT X, INT Y);                                 // 0x1015d430 (ret 8) a person of the tree opens a chat

    // what the protocol object tells the window (names ours; the strings and records are given by value and freed by the callee)
    __declspec(noinline) void OnNewMail(NMsnStringArg Subject);      // 0x10155c60 (ret 0xC) the mail alarm, when it is wanted
    __declspec(noinline) void OnSignedIn(NMsnPersonRec Self);        // 0x10155ef0 (ret 0x44) the sign-in is done: the pages swap, Me is Self
    __declspec(noinline) __declspec(safebuffers) void SetMe(NMsnPersonRec Self);             // 0x10155fd0 (ret 0x44)
    __declspec(noinline) void ShowAddedConfirm(NMsnPersonRec Person);   // 0x10158180 (ret 0x44) somebody added us: the box that asks
    __declspec(noinline) void RefreshGroupOf(NMsnPersonRec Person);  // 0x10158790 (ret 0x44) the group of this person is filled again
    __declspec(noinline) void OnFriendRequest(NMsnPersonRec Person); // 0x10158800 (ret 0x44) the box of ShowAddedConfirm unless "don't disturb"
    __declspec(noinline) void OnFriendPresence(NMsnPersonRec Person);   // 0x1015bb10 (ret 0x44) a person's record changed: his group is filled
                                                                     //   again, the alarm may show, the chat windows are told
    __declspec(noinline) void RefreshFriend(NMsnPersonRec Person);   // 0x1015bc80 (ret 0x44) the same without the alarm
    __declspec(noinline) void ShowChatOfSession(void* Session);      // 0x1015cd90 (ret 4) the chat window of the account of this session is looked up
    __declspec(noinline) void OnChatClosed(NCWnd* Chat);             // 0x1015d380 (ret 4) a chat window closed: it leaves the tables, the task window and the protocol object
    __declspec(noinline) void ResetAfterSignOut();                   // 0x1015ca80 the chat windows put away, the tables and the tree emptied
    __declspec(noinline) void OnSignedOut(NMsnStringArg Reason);     // 0x1015d180 (ret 0xC) the protocol object signed out: the log-in page is back
    // 0x10133BE0 (ret 0xC; the code by value, destroyed on return; was anonymous) the dialog for an error number of the server ("201" "205"
    // "207" "911" "930"); the last two also put the log-in page back
    void ShowError(FString Code);
    // 0x10138460 (ret 4; was anonymous) a switchboard session was taken out of the manager: the chat window of its id (second table)
    // forgets the session (its own and its invite window's) and the id leaves the table
    void OnSessionRemoved(SwitchboardSession* Session);
};
NW_ASSERT_OFFSET(MsnWnd, MsnObj, 0xE8);
NW_ASSERT_OFFSET(MsnWnd, ChatWnds, 0xEC);
NW_ASSERT_OFFSET(MsnWnd, ChatNames, 0x100);
NW_ASSERT_OFFSET(MsnWnd, InviteWnds, 0x114);
NW_ASSERT_OFFSET(MsnWnd, Me, 0x128);
NW_ASSERT_OFFSET(MsnWnd, SignOutTimer, 0x16C);
NW_ASSERT_OFFSET(MsnWnd, LoginWindow, 0x170);
NW_ASSERT_OFFSET(MsnWnd, TreeCtrl, 0x17C);
NW_ASSERT_OFFSET(MsnWnd, L2FriendWindow, 0x180);
NW_ASSERT_OFFSET(MsnWnd, OptionWindow, 0x184);
NW_ASSERT_OFFSET(MsnWnd, StatusWindow, 0x18C);
NW_ASSERT_OFFSET(MsnWnd, AddFriendDialog, 0x190);
NW_ASSERT_OFFSET(MsnWnd, MessageDialog, 0x1B0);
NW_ASSERT_OFFSET(MsnWnd, StatusButton, 0x1B4);
NW_ASSERT_OFFSET(MsnWnd, SignOutButton, 0x1C8);
NW_ASSERT_OFFSET(MsnWnd, BackTex, 0x1CC);
NW_ASSERT_SIZE(MsnWnd, 0x1D0);

// The protocol object's calls that take FStrings by value (stock functions, free what they are given): the argument is made in
// the slot as the stock callers make it (the text built inline in the caller, as the stock does: the *Arg functions get it as the
// callee's own copy). The *Arg functions are out of line (MsnWnd.cpp), where the protocol lane's classes are in sight
void MsnSetStatusArg(Msn* M, NMsnStringArg Code);                         // 0x1014F550 (ret 0xC)
void MsnSetDisplayNameArg(Msn* M, NMsnStringArg Name);                    // 0x1014F5C0 (ret 0xC)
void MsnSignIn(Msn* M, NMsnStringArg Account, NMsnStringArg Password);    // 0x10150120 (ret 0x18)
void MsnBlockArg(Msn* M, NMsnStringArg Account, INT bBlock);              // 0x1014F300 (ret 0x10)
void* MsnFindInList(void* List, NMsnStringArg Account);                   // 0x101515E0 (ret 0xC): the person record, NULL for none
NMsnPersonRec* MsnFindFriendArg(Msn* M, NMsnStringArg Account);           // 0x1014FC30 (ret 0xC)
FORCEINLINE void MsnSetStatus(Msn* M, const TCHAR* Code) { MsnSetStatusArg(M, MsnStringArgOf(Code)); }
FORCEINLINE void MsnSetDisplayName(Msn* M, const TCHAR* Name) { MsnSetDisplayNameArg(M, MsnStringArgOf(Name)); }
FORCEINLINE void MsnBlock(Msn* M, const TCHAR* Account, INT bBlock) { MsnBlockArg(M, MsnStringArgOf(Account), bBlock); }
FORCEINLINE void* MsnFindInList(void* List, const TCHAR* Account) { return MsnFindInList(List, MsnStringArgOf(Account)); }
// The small getters of the protocol object and of its person lists (called, not inlined, by the stock code)
void* MsnGetLists(Msn* M);                                                // 0x1014FC10 [M + 0x50] the person lists
NMsnPersonRec* MsnGetSelf(Msn* M);                                        // 0x1014FC20 [M + 0x54] the signed-in person
// C4: the getters of the five fields of the lists (folded with the getters of other classes): FL [+0] 0x1016EC20, AL [+4] 0x10151140, BL [+8]
// 0x10005B90, RL [+0xC] 0x10151150, Groups [+0x10] 0x10151160 (OnBlockButton calls 0x10005B90, OnMoveGroupButton 0x1016EC20)
#if NW_HOOK_MODE
FORCEINLINE void* MsnListFriends(void* Lists) { return ::nw::StockCallAt<void*>(0x1016EC20, Lists); }  // [Lists + 0] the person list (FL)
#else
FORCEINLINE void* MsnListFriends(void* Lists) { return *(void**)Lists; }   // [Lists + 0]: Shared::GetFirstDword_ICF 0x1016EC20 (3 bytes)
#endif
void* MsnListBlocked(void* Lists);                                        // 0x10005B90 [Lists + 8] the block list (BL)
FORCEINLINE NMsnPersonRec* MsnFindFriend(Msn* M, const TCHAR* Account) { return MsnFindFriendArg(M, MsnStringArgOf(Account)); }

// The messenger's dialogs are made and called by the files of their own: the headers of the dialogs (NMsnDlgCommon.h and its family) and
// the protocol headers MsnWnd.cpp includes define the same names (NMsnGroup, NMsnCommand) and cannot be in one file. Each function is
// defined in the .cpp of its dialog; Link is the protocol object (Msn*, seen by the dialogs as NMsnLink*), Owner the MsnWnd.
NCWnd* MsnNewAddFriendDialog(void* Link);                    // AddFriendDialog(NMsnLink*)              0x10148530
NCWnd* MsnNewRemoveFriendDialog(void* Owner, void* Link);    // RemoveFriendDialog(MsnWnd*, NMsnLink*)  0x101485F0
NCWnd* MsnNewMoveGroupDialog(void* Link);                    // MoveGroupDialog(NMsnLink*)              0x101487F0
NCWnd* MsnNewAddedConfirmDialog(void* Link);                 // AddedConfirmDialog(NMsnLink*)           0x10148700
NCWnd* MsnNewOfflineConfirmDialog(void* Owner);              // OfflineConfirmDialog(MsnWnd*)           0x10148470
NCWnd* MsnNewAddGroupDialog(void* Link);                     // AddGroupDialog(NMsnLink*)               0x10148900
NCWnd* MsnNewRenameGroupDialog(void* Link);                  // RenameGroupDialog(NMsnLink*)            0x101489C0
NCWnd* MsnNewRemoveGroupDialog(void* Owner, void* Link);     // RemoveGroupDialog(MsnWnd*, NMsnLink*)   0x10148A80
NCWnd* MsnNewMessageDialog();                                // MessageDialog()                         0x10148B50
// AddedConfirmDialog::AddBuddy 0x101473D0: a copy of the record is queued (the box keeps its own)
void MsnAddedConfirmAddBuddy(NCWnd* Dialog, const NMsnPersonRec& Person);
// and what the dialog of "appear offline" asks of the window (MsnWnd.cpp, where the protocol classes are in sight)
void MsnChatResetSession(NCWnd* Chat);                       // ChatWnd::ResetSession 0x10142EC0
void MsnTaskWndDestroyTasks(NCWnd* TaskWindow);              // NChatTaskWnd::DestroyTasks 0x10141C90

// The text fitters of the messenger (free functions, cdecl, names ours)
INT MsnFitLine(const TCHAR* Text, TCHAR* Out, INT MaxWidth, INT EllipsisReserve);   // 0x10144050
void MsnFitText(const TCHAR* Text, TCHAR* Out, INT MaxWidth, INT MaxHeight);        // 0x10144150

#pragma pack(pop)
