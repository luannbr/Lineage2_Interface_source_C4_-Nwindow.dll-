#pragma once
// NCTabControl - row of tab buttons, each showing one page window (UClass "CTabControl",
// 0x1C4). The tabs are NCRadioButtons whose ID is the control's GroupID and whose Value is
// the tab index: a click comes back as NWM_COMMAND(GroupID, index) and selects that page.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRadioButton;

#if NW_HOOK_MODE
#define GNextTabGroupID NW_GLOBAL(INT, 0x1022A7F8)   // starts at 0xFA0000 (name ours)
#else
extern INT GNextTabGroupID;
#endif

class NCTabControl : public NControl
{
    NW_DECLARE_CLASS(NCTabControl, NControl, 0, 0x102C5420)

    INT NextTabX;                    // +0x1F4 X of the next tab button
    TArray<NCRadioButton*> Tabs;     // +0x1F8
    INT GroupID;                     // +0x204 ID of the tab buttons (OnCreate)
    NCWnd* CurrentPage;              // +0x208
    TArray<NCWnd*> Pages;            // +0x20C one per tab (not owned)
    // C4 (name ours): bit 0 = the tabs can be dragged off the bar (AddTab's last argument gives them NWS_DRAGABLE; the page
    // follows the mouse until the tab is let go over the bar again); the constructor clears it, the owner sets it
    INT Flags;                       // +0x218

    __declspec(noinline) NCTabControl();                             // 0x1002e750
    __declspec(noinline) NCTabControl(INT InFlags);                  // 0x1002e860 (ret 4; anonymous in the db) the same with Flags given (NCChatWnd::OnCreate)
    // ~NCTabControl                                                 // 0x1002e7f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's)
    virtual INT OnCreate();                                          // 37 0x1002d6a0
    virtual INT OnDestroy();                                         // 38 0x1002e390
    virtual void ShowCurrentPage();                                  // 75 0x1002e330 (name ours)
    // C4: the owner is told only when bNotify (the tab buttons pass 1); the stock callers pass both arguments
    virtual INT SelectTab(INT Index, INT bNotify = 1);               // 76 0x1002e550 (name ours)
    virtual void NewVirtual77();  // 77 0x1002da20 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual78();  // 78 0x1002db50 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // names ours; a tab of W x H 0 is sized to its label. The page of the first tab is the current one; the tabs are NCTabButtons
    // (rest1's class, built by its stock constructor 0x10003b00) kept as NCRadioButtons here. The stock callers of the first pass
    // nine arguments (the last gives the tab NWS_DRAGABLE: the tabs can then be torn off, see Flags)
    INT AddTab(INT TextID, NCWnd* Page, INT W, INT H, const TCHAR* NormalTexName,
               const TCHAR* PushedTexName, INT Gap, INT ToolTipID, INT bDragable = 0);  // 0x1002ded0 (ret 0x24)
    INT AddTab(const TCHAR* Text, NCWnd* Page, INT W, INT H, const TCHAR* NormalTexName,
               const TCHAR* PushedTexName, INT Gap, INT ToolTipID);  // 0x1002e120 (ret 0x20)
    void SetTab(INT Index, BYTE bNotify = 1);                        // 0x1002dd70 silent SelectTab (C3: the owner is told only when bNotify)
    TArray<NCRadioButton*> GetTabs();                                // 0x1002e720 (ret 4, the hidden return pointer)
    // C4 (names ours). A "split" tab is one dragged off the bar: its page is shown on its own
    void SetDisable(INT Index, INT bDisable);                        // 0x1002d7a0 (ret 8, guard) the tab goes grey (and, with Flags, not draggable)
    INT IsSplit(INT Index);                                          // 0x1002d880 (ret 4, guard)
    INT IsPageSplit(NCWnd* Page);                                    // 0x1002e4a0 (ret 4, guard; the stock name is IsSplit too)
    void DefaultPosition();                                          // 0x1002d930 (guard) every split tab goes back to the bar
    // the bodies of slots 39, 77 and 78, hooked by their addresses under these names (the slots' own definitions are the
    // generated stubs of NNewVirtuals.gen.cpp)
    INT TabMove(INT Index, INT X, INT Y);                            // 0x1002da20 (slot 77, ret 0xC, guard OnTabMove) a tab is being dragged
    INT TabRelease(INT Index);                                       // 0x1002db50 (slot 78, ret 4, guard OnTabRelease) it was let go
    void MoveTabs(INT DX, INT DY);                                   // 0x1002dcc0 (slot 39, ret 8, guard OnMove) the window moves: the tabs' bar positions follow
    virtual void NewVirtual39();  // 39 0x1002dcc0 (the stock vtable of this class overrides the placeholder of a parent; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCTabControl, Tabs, 0x1F8);
NW_ASSERT_OFFSET(NCTabControl, GroupID, 0x204);
NW_ASSERT_OFFSET(NCTabControl, Pages, 0x20C);
NW_ASSERT_OFFSET(NCTabControl, Flags, 0x218);
NW_ASSERT_SIZE(NCTabControl, 0x21C);

#pragma pack(pop)