#pragma once
// NCMenuWnd - the main menu bar (UClass "CMenuWnd", 0xA8): four big icons, each opening or
// closing one of the desktop's windows (Character Status Alt+I, Inventory Alt+V, Community
// Alt+B, System Alt+X), and a thin title bar on the left to drag it.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NWM_COMMAND ids of the buttons and the title bar
enum
{
    NID_MENU_INVENTORY = 0xF0F000,
    NID_MENU_CHARACTER = 0xF0F001,
    NID_MENU_SYSTEM = 0xF0F003,
    NID_MENU_COMMUNITY = 0xF0F006,
    NID_MENU_FRAME = 0xF0F021,
};

class NCMenuWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMenuWnd, NCWnd, 0, 0x102E73C8)

    UTexture* BackTex;                     // +0xE8 "L2UI_ch3.SmallWnd.Smallwindow_back2" (C3): the middle, stretched
    UTexture* BackLeftTex;                 // +0xEC "L2UI_ch3.SmallWnd.Smallwindow_back1": the left end, at (12, 0) 16 x 46
    UTexture* BackRightTex;                // +0xF0 "L2UI_ch3.SmallWnd.Smallwindow_back3": the right end, at Width - 16
    BYTE _UnknownD4[0xD8 - 0xD4]; // +0xF4 (nothing touches it)   // C2: offset derived, no evidence
    NCButton* SystemButton;                // +0xF8
    NCButton* InventoryButton;             // +0xFC
    NCButton* CharacterButton;             // +0x100
    NCButton* CommunityButton;             // +0x104

    NCMenuWnd();                                                     // 0x100b8370
    // ~NCMenuWnd                                                    // 0x10098130 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b6920
    virtual INT OnCreate();                                          // 37 0x100b7de0
    virtual INT OnDestroy();                                         // 38 0x100b7360 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b7260
    // names ours
    virtual INT OnSystem();                                          // 74 0x100b69d0
    virtual INT OnCharacter();                                       // 75 0x100b69f0
    virtual INT OnInventory();                                       // 76 0x100b6a10
    // 71 0x100aa0a0 is the Community button's handler (WindowProc calls it with vcall +0x11c); it keeps the
    // NewVirtual71 name because NNewVirtuals.gen.cpp (generated) defines it; the body is OnCommunity below
    virtual void NewVirtual77();  // 77 0x100b6a30 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnCommunity();                                       // 0x100b6a30: the body of vslot 71 (non-virtual here, see above)
};
NW_ASSERT_OFFSET(NCMenuWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMenuWnd, CommunityButton, 0x104);
NW_ASSERT_SIZE(NCMenuWnd, 0x108);

#pragma pack(pop)