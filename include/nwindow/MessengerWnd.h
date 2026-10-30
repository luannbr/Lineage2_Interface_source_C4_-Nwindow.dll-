#pragma once
// MessengerWnd - the window that holds the messenger (UClass "CMessengerWnd", 0xD8; C3 only, no C2 source): a title bar, the
// two pages (MsnWnd, the MSN messenger, and L2FriendWnd, the Lineage II friend list) and the two tabs that choose between them,
// over a strip of picture. When it is minimized its icon shows the presence of the signed-in MSN person (a different
// little picture for online, busy, away and offline). The stock console code makes it (the "friends" button).
//
// Layout and virtuals from the stock constructor 0x101395A0, destructor 0x10139600, OnCreate 0x1013B850, OnDestroy 0x10133FF0,
// OnPaint 0x10134090, MinimizeWindow 0x10134300, ShowWindow 0x10134500 and SetFocus 0x10134570.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnWnd;
class L2FriendWnd;
class NCTabControl;

class MessengerWnd : public NCWnd
{
    NW_DECLARE_CLASS(MessengerWnd, NCWnd, 0, 0x103231A0)

    MsnWnd* MsnPage;                 // +0xE8 the MSN messenger, 256 x 353 at (0, 48), the first tab (name ours)
    L2FriendWnd* FriendPage;         // +0xEC the Lineage II friends, the same place, the second tab (name ours)
    NCTabControl* Tabs;              // +0xF0 id 0xA02, 224 x 41 at (12, 25) (name ours)
    UTexture* BackTex;               // +0xF4 "L2UI_CH3.Msn.msn_back" (name ours)

    MessengerWnd();                                                  // 0x1015df10
    // ~MessengerWnd                                                 // 0x1015df70

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, shared)
    virtual void SetFocus();                                         // 29 0x10159220
    virtual INT OnCreate();                                          // 37 0x10160240
    virtual INT OnDestroy();                                         // 38 0x10158c90
    virtual void ShowWindow();                                       // 48 0x101591a0
    virtual void Minimize();                                         // 54 0x10158fa0 (the db says MinimizeWindow)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10158d30

    // names ours (anonymous in the db)
    __declspec(noinline) INT IsIconActive();                         // 0x10159260 the icon of the minimized window is the one in use
    __declspec(noinline) void SetMinimizedIcon(const TCHAR* IconName);   // 0x10156130 (ret 4) the icon takes this little picture
};
NW_ASSERT_OFFSET(MessengerWnd, MsnPage, 0xE8);
NW_ASSERT_OFFSET(MessengerWnd, FriendPage, 0xEC);
NW_ASSERT_OFFSET(MessengerWnd, Tabs, 0xF0);
NW_ASSERT_OFFSET(MessengerWnd, BackTex, 0xF4);
NW_ASSERT_SIZE(MessengerWnd, 0xF8);

// The two helpers of the version check (names ours, free functions, cdecl): the version of the installed MSN messenger as text
// ("a.b.c.d", "0.0.0.0" when there is none) from the registry, and the version resource of a file
__declspec(noinline) INT MsnGetMessengerVersion(TCHAR* Out);                              // 0x10144770 (1 when found)
__declspec(noinline) INT MsnGetFileVersion(const TCHAR* Path, DWORD* Ms, DWORD* Ls);      // 0x10144330
__declspec(noinline) void MsnCheckVersion();                                              // 0x101314F0 sets the "blocked" flag when it is older than 6.2.0.205

#pragma pack(pop)
