#pragma once
// MsnUiStock.h - stage 5 (the standalone NWindow.dll), lane sa-msn-ui. New file.
//
// The messenger windows call functions of other files by the address they have in the stock image (StockCallAt / nw::Addr). Those functions
// are ported already, but most of them hang on a small stand-in struct that lives in the .cpp of the file that hooks them (the stock has no
// class for them: "NMsnCtl", "NFoldedDwordAt50", "NConsoleForwardFns"...). A typed call needs the declaration, so the ones this lane calls are
// declared here, by the SAME struct name, method name and signature as the definition (the mangled name must be the same, the definitions are
// not touched): the call is then the call of the function that holds the address, in the hook build and in the standalone.
//
// Nothing here has a body or a layout of its own: every struct is only a name for `this`, reached by a cast of the object the stock code
// calls the function on. When the owner of a definition changes its name or signature the link of the standalone says it (an unresolved
// external naming the stand-in): the declaration here follows. Where the definition lives is in the comment of each declaration.
//
// This header is neutral (it needs only MsnWnd.h's small types): the stand-ins that derive from the protocol lane's classes are in
// MsnUiStockP.h.

#include "nwindow/NCWnd.h"
#include "nwindow/NControl.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NConsoleFns.h"   // NConsoleLeftFns / NConsoleForwardFns (one definition for all the headers, stage 5 sa-verify)
#pragma pack(push, 4)

class L2ParamStack;

// NCoreHelpers.cpp (a core lane's file; the two lost their `static` for this): Util::MsnStrcmp and Util::MsnCompareFriend, cdecl, the strings
// by value (the function destroys them)
INT __cdecl NUtilMsnStrcmp(FString A, FString B);                                           // 0x10168DD0
INT __cdecl NUtilMsnCompareFriend(FString NameA, INT OnlineA, FString NameB, INT OnlineB);   // 0x101692E0

// NCoreHelpers.cpp: the skin of an icon / a button, ret 0xC; the flag is a byte there
struct NCoreSkinControl : public NControl
{
    void SetSkin(const TCHAR* TexName, const TCHAR* Text, ANSICHAR Flag);        // 0x1001DB90 (ret 0xC)
};

// NPetitionFeedBackWnd.cpp (NCPetitionFeedBackWnd.cpp): the four textures of a check box by name, ret 0x10 (an inline member there)
struct NCCheckBoxTexturesView : public NCCheckBox
{
    __declspec(noinline) void SetTextures(const TCHAR* Checked, const TCHAR* Unchecked, const TCHAR* UncheckedDisabled,
                                          const TCHAR* CheckedDisabled);   // 0x10005290 (ret 0x10; defined and hooked in NCPetitionFeedBackWnd.cpp)
};

// (NMsnCtl, the thin calls of the protocol object that go to its session, is gone: MsnLogin.cpp defines them as Msn::SignOut / Block /
// SetStatus / SetDisplayName, and no file called the stand-in)

// NLeftHelpers.cpp: the getters of one dword that the linker folded between classes (`this` is the object whose field is read; the
// one-line getters `mov eax, [ecx + N]` / `ret`, as members of a stand-in struct so that regcheck sees `this` in ecx come from a member).
// The one definition (NLeftHelpers.cpp used to repeat it with the data members)
struct NFoldedDwordAt08
{
    BYTE Pad[8];
    INT Value;
    __declspec(noinline) INT Get();                                              // 0x10005B90 [this + 8]
};
struct NFoldedDwordAt50
{
    BYTE Pad[0x50];
    INT Value;
    __declspec(noinline) INT Get();                                              // 0x1014FC10 [this + 0x50] (Msn: the person lists)
};

// (stage 5, sa-verify: NMsnStrMapOps, NMsnListInit, NMsnGroupsInit are defined once in MsnProtoOps.h, NMsnTaskMapOps in MsnWndOps.h, with the
// protocol classes they derive from; this header, which the dialogs' files can include, no longer repeats them)

// L2FriendWnd.cpp: the friend book, the 0x20-byte object the constructor makes (0x101525f0) and the L2 friend window keeps: an array, empty, and
// a table by name, empty (names ours); Find is the record of the friend of that name, NULL for none (the name, passed by value, is freed by the
// function). Stage 5 (sa-verify): the one definition (the console's headers declared Find alone, and L2FriendWnd.cpp the whole struct)
struct NL2Book
{
    INT ArrayData;                   // +0x00
    INT ArrayNum;                    // +0x04
    INT ArrayMax;                    // +0x08
    NMsnWndStrMap Names;             // +0x0C

    NL2Book() : ArrayData(0), ArrayNum(0), ArrayMax(0) {}
    __declspec(safebuffers) void* Find(NMsnStringArg Name);          // 0x10151670 (ret 0xC)
};
NW_ASSERT_SIZE(NL2Book, 0x20);

// ChatWnd.cpp: the task list: the line whose Param is that task window leaves the list (TaskBox's and L2TaskBox's, folded)
struct NChatTaskListBox : public NCListBox
{
    void RemoveTask(NCWnd* Task);   // 0x10149a70 (ret 4)
};

// An L2Friend passed by value, as the stock callee gets it: six words that the callee owns (it destroys the name). Plain
// words, so that a hook does not copy the string
struct NL2FriendArg
{
    DWORD W[6];
};
NW_ASSERT_SIZE(NL2FriendArg, 0x18);

struct NMsnMainPanel;

// NConsoleWndMisc.cpp: the messenger window (the desktop's +0x18C): its main panel is the child at +0xEC
struct NMsnShellWnd : public NCWnd
{
    void FriendAdd(NL2FriendArg Arg);      // 0x10134610 (ret 0x18)
    void FriendRemove(NL2FriendArg Arg);   // 0x10134680 (ret 0x18)
    void FriendUpdate(NL2FriendArg Arg);   // 0x101346F0 (ret 0x18)
    __declspec(noinline) void ReloadFriends();                  // 0x101561a0
    __declspec(noinline) void PanelCall139370();                // 0x1015dfe0: the panel's CloseAll
    FString* GetPeerName(FString* Result);   // 0x10159280 (ret 4)
    __declspec(noinline) void PanelChat(INT A, TCHAR* Name, TCHAR* Text);   // 0x1015dff0 (ret 0xC): the panel's OnChat

    NMsnMainPanel* MainPanel() { return nw::Field<NMsnMainPanel*>(this, 0xEC); }
};

// (NConsoleLeftFns / NConsoleForwardFns, the small methods of the console the linker kept apart, are defined once in NConsoleFns.h)

#pragma pack(pop)
