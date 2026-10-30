#pragma once
// The GM's windows. NCGMWnd (UClass "CGMWnd", 0x118; desktop +0x190, //gmwnd): a player's name
// and the buttons that ask the server about that player and open the matching view window of
// the desktop (NCConsole::ToggleGM*): Status (NCGMDetailStatusWnd), Inventory, Skills
// (NCGMMagicSkillWnd), Quest, Warehouse (NCGMStoreWnd) and Clan; the builder commands of the other
// buttons (teleportto, add_peti_chat, debug, instant_move), the searches of an NPC, an item and a
// skill by name, and one that closes the view windows.
//
// The view windows are the player's windows under a title bar, filled by the console with the
// server's answer and made read-only: nothing is dragged out of their grids (the item windows'
// release and right button only put the carried item down), the quest window's Cancel is
// disabled, the clan page's buttons are all disabled.
//   NCGMInventoryWnd (0xE4; desktop +0x194): the inventory's paperdoll and two pages with the
//     adena, no weight bar, no trash or crystallize icons.
//   NCGMQuestWnd (0x18C; desktop +0x1A0; C3: NCQuestGaraTreeWnd's 0x180 bytes and a list of item counts),
//     NCGMActionWnd (0xCC; desktop +0x1A8, the clan page NCGMPledgeWnd only).

#include "nwindow/NCEquipItemWnd.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NCPledgeWnd.h"
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NCQuestWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCEditBox;
class NCTabControl;
struct NItemInfo;

// NWM_COMMAND ids (names ours)
enum
{
    NID_GM_NAME = 0x1505,
    NID_GM_INVENTORY = 0xF0F006,
    NID_GM_STATUS = 0xF0F007,
    NID_GM_SKILLS = 0xF0F008,
    NID_GM_QUEST = 0xF0F00A,
    NID_GM_CLAN = 0xF0F00B,
    NID_GM_WAREHOUSE = 0xF0F00C,
    NID_GM_FRAME = 0xF0F015,
    NID_GM_FORCEPETI = 0xF0F016,      // C4: the C3 close button's id; the window has no close handler any more
    NID_GM_CHANGESERVER = 0xF0F123,   // new in C4 (name ours, from the handler's GL2DisableGCLobby)
    NID_GM_NEW1 = 0xF0F00D,           // new in C2: teleportto
    NID_GM_NEW7 = 0xF0F00E,           // C3, name ours: the dialog 0x4B (C4 slot 82)
    NID_GM_NEW2 = 0xF0F03F,           // add_peti_chat (C4 slot 88)
    NID_GM_NEW8 = 0xF0F03D,           // instant_move (C4 slot 86)
    NID_GM_NEW3 = 0xF0F05D,           // debug (C4 slot 89)
    NID_GM_NEW9 = 0xF0F03E,           // the dialog 0x4A (C4 slot 87)
    NID_GM_NEW4 = 0xF0F01C,
    NID_GM_NEW5 = 0xF0F02C,
    NID_GM_NEW6 = 0xF0F03C,
    NID_GMINVENTORY_FRAME = 0xF0F00D,
    NID_GMINVENTORY_TABS = 0xF0F00E,
    NID_GMACTION_FRAME = 0xF0F018,
    NID_GMQUEST_FRAME = 0xF0F119,
    NID_GMQUEST_TABS = 0xF0F120,
    NID_GMQUEST_CANCEL = 0xF0F121,     // disabled, and the window procedure ignores it
    NID_GMQUEST_LIST = 0xF0F122,       // not handled: a quest clicked shows nothing
};

class NCGMWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMWnd, NCWnd, 0, 0x102D6AA0)

    // C2: not placed (no evidence for where it went): UTexture* TopTex;                // +0xC8 "sek.ui02"
    // C2: not placed (no evidence for where it went): UTexture* MidTex;                // +0xCC "sek.cbui157"
    UTexture* BottomTex;             // +0xE8 C4: the back picture, "l2ui_ch3.siegewnd.siege_back31" (C3: the bottom of three "sek" pictures)
    NCEditBox* NameEdit;             // +0xEC the player's name (ToggleGMWnd focuses it)
    NCButton* InventoryButton;       // +0xF0 id 0xF0F006, sysstring 138
    NCButton* SkillsButton;          // +0xF4 id 0xF0F008, sysstring 119
    NCButton* StatusButton;          // +0xF8 id 0xF0F007, sysstring 433
    NCButton* QuestButton;           // +0xFC id 0xF0F00A, sysstring 118
    NCButton* WarehouseButton;       // +0x100 id 0xF0F00C, sysstring 131
    NCButton* ClanButton;            // +0x104 id 0xF0F00B, sysstring 439
    // C3: the buttons the C2 client added are fewer / more: nine, in the order OnCreate makes them (names ours)
    NCButton* NewButton1;            // +0x108 id 0xF0F00D, sysstring 899 (slot 75: teleportto)
    NCButton* NewButton7;            // +0x10C id 0xF0F00E, text 688 (slot 76)
    NCButton* NewButton2;            // +0x110 id 0xF0F03F, sysstring 902 (C4 slot 88: add_peti_chat)
    NCButton* NewButton8;            // +0x114 id 0xF0F03D, sysstring 900 (C4 slot 86: instant_move)
    NCButton* NewButton3;            // +0x118 id 0xF0F05D, text 901 (C4 slot 89: debug)
    NCButton* NewButton9;            // +0x11C id 0xF0F03E, text 903 (C4 slot 87)
    NCButton* NewButton4;            // +0x120 id 0xF0F01C, text 690 (slot 77: NPC by name)
    NCButton* NewButton5;            // +0x124 id 0xF0F02C, text 691 (slot 78: item by name)
    NCButton* NewButton6;            // +0x128 id 0xF0F03C, text 692 (slot 79: skill by name)
    NCButton* ForcePetiButton;       // +0x12C id 0xF0F016 (slot 80: "force_peti"; C3: the close button), text 1274
    NCButton* ChangeServerButton;    // +0x130 id 0xF0F123 (slot 90, new in C4), text 1275

    NCGMWnd();                                                       // 0x100907e0
    // ~NCGMWnd                                                      // 0x10090860

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10089410
    virtual INT OnCreate();                                          // 37 0x1008dac0
    virtual INT OnDestroy();                                         // 38 0x1008d0c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008d450
    // the buttons (names ours)
    virtual INT OnInventory();                                       // 74 0x10089650
    virtual INT OnSkills();                                          // 75 0x100896b0
    virtual INT OnStatus();                                          // 76 0x10089710
    virtual INT OnQuest();                                           // 77 0x10089770
    virtual INT OnWarehouse();                                       // 78 0x100897d0
    virtual INT OnClan();                                            // 79 0x10089830
    virtual INT OnForcePeti();                                       // 80 0x10089890 (name ours; the C3 slot was OnClose) "force_peti <name> <sysmsg 1528>"
    // the builder commands "teleportto", "add_peti_chat", "debug" and "instant_move" for the name typed (1, 2, 3 and 8), two
    // dialog boxes asking about it (7: sysmsg 0x4C4, 9: 0x4C5) and the searches of an NPC, an item and a skill by
    // name (4 .. 6: the match's id and name in a dialog box)
    virtual INT OnNewButton1();                                      // 81 0x10089980 (id 0xF0F00D)
    virtual void NewVirtual82();  // 82 0x1008d530 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnNewButton4();                                      // 83 0x1008eeb0 (id 0xF0F01C) NPC table (GL2GameData +0x180C)
    virtual INT OnNewButton5();                                      // 84 0x1008f020 (id 0xF0F02C) item table (+0x1808)
    virtual INT OnNewButton6();                                      // 85 0x1008f180 (id 0xF0F03C) skill table (+0x3CDC)
    virtual void NewVirtual86();  // 86 0x10089a40 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual87();  // 87 0x1008d5f0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual88();  // 88 0x10089ae0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual89();  // 89 0x10089ba0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual90();  // 90 0x10089c60 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // the bodies behind the placeholders (slots 76, 80 .. 83; WindowProc calls the slots)
    INT OnNewButton7();                                              // 0x1008d530 (name ours) slot 82: dialog 0x4B, sysmsg 0x4C4 with the name
    INT OnNewButton8();                                              // 0x10089a40 (name ours) slot 86: the builder command instant_move
    INT OnNewButton9();                                              // 0x1008d5f0 (name ours) slot 87: dialog 0x4A, sysmsg 0x4C5 with the name
    INT OnGMChangeServerBtnClick();                                  // 0x10089c60 (the db's name) slot 90 (NewVirtual90): the GM changes the server of the name's number
    INT OnNewButton2();                                              // 0x10089ae0 (id 0xF0F03F) slot 88
    INT OnNewButton3();                                              // 0x10089ba0 (id 0xF0F05D) slot 89
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCGMWnd, NameEdit, 0xEC);
NW_ASSERT_OFFSET(NCGMWnd, ClanButton, 0x104);
NW_ASSERT_OFFSET(NCGMWnd, NewButton3, 0x118);
NW_ASSERT_OFFSET(NCGMWnd, ForcePetiButton, 0x12C);
NW_ASSERT_OFFSET(NCGMWnd, ChangeServerButton, 0x130);
NW_ASSERT_SIZE(NCGMWnd, 0x134);

// the GM inventory's pages (UClass "CGMInvenItemWnd", 0x164)
class NCGMInvenItemWnd : public NCInvenItemWnd
{
    NW_DECLARE_CLASS(NCGMInvenItemWnd, NCInvenItemWnd, 0, 0x102D7A38)

    NCGMInvenItemWnd();                                              // 0x1008f6f0
    // C3: the four-argument one is NCInvenItemWnd's six-argument constructor with the gaps 1 and 1 (ret 0x10)
    NCGMInvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x1008f790
    // ~NCGMInvenItemWnd                                             // 0x1008f740

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10089070
    // NCGMSkillItemWnd::OnLButtonUp's body (0x1007e280, hooked there): unhooked copies
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10089090 (folded)
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x10089090 (folded)
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x10089090 (folded)
};
NW_ASSERT_SIZE(NCGMInvenItemWnd, 0x180);

// the GM inventory's paperdoll (UClass "CGMEquipItemWnd", 0x28C)
class NCGMEquipItemWnd : public NCEquipItemWnd
{
    NW_DECLARE_CLASS(NCGMEquipItemWnd, NCEquipItemWnd, 0, 0x102D7508)
    BYTE bFirstRingTaken;                 // +0x2A4 (name ours; 0 from the constructor and from NCGMInventoryWnd::ClearItems) AddItem:
                                          //   the first item with the body part bit 2 / 4 (the earrings) keeps bit 4 and sets it
    BYTE bFirstFingerTaken;               // +0x2A5 (the same) bits 0x10 / 0x20 (the rings)
    BYTE _UnknownTail28A[0x28C - 0x28A];  // +0x2A6 padding up to sizeof   // C2: offset derived, no evidence

    __declspec(noinline) NCGMEquipItemWnd(INT InCellSize = 32);      // 0x1008f640 (the stock code calls it out of line)
    // ~NCGMEquipItemWnd                                             // 0x1008f6a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10088f30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10089dd0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10088f50
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x10088f50 (folded)
    virtual INT GetEquipSlot(INT BodyPart, INT ObjectID);            // 74 0x10088f70 the slot by the body part bits alone
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 75 0x10088f50 (folded)

    // names ours (the console's adds go through NCGMInventoryWnd::AddItem)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x1008d6b0 NCEquipItemWnd's, but a second ring (earring)
                                                                     //   is shown on the other slot
};
NW_ASSERT_OFFSET(NCGMEquipItemWnd, bFirstRingTaken, 0x2A4);
NW_ASSERT_SIZE(NCGMEquipItemWnd, 0x2A8);

class NCGMInventoryWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMInventoryWnd, NCWnd, 0, 0x102D89D0)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.InventoryWnd.Inventory_Back"
    UTexture* AdenaTex;              // +0xEC "L2UI.WindowIcon.WindowIconAdena"
    UTexture* WeightTex;             // +0xF0 "L2UI.WindowIcon.Weight" (name ours; drawn at the bottom, no bar)
    BYTE _UnknownF4[0xF8 - 0xF4]; // +0xF4 (new in C2)
    NCGMInvenItemWnd* ItemWnd[2];    // +0xF8 the pages: 0 "Item", 1 "Quest"
    NCGMEquipItemWnd* EquipWnd;      // +0x100
    NCTabControl* TabCtrl;           // +0x104

    NCGMInventoryWnd();                                              // 0x1008f7c0
    // ~NCGMInventoryWnd                                             // 0x1008f820

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x1008f870
    virtual INT OnDestroy();                                         // 38 0x1008a830
    virtual void ShowWindow();                                       // 48 0x10089100
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008a5a0

    // the console's calls (names ours)
    void ClearItems();                                               // 0x100890b0
    UBOOL AddItem(NItemInfo* Info);                                  // 0x1008d950
    void AddHenna(INT HennaID, INT bUsable);                         // 0x10089120 (name ours) the paperdoll's (a tail jump)
    void ClearHennas();                                              // 0x10089130 (name ours) the paperdoll's (a tail jump)
};
NW_ASSERT_OFFSET(NCGMInventoryWnd, ItemWnd, 0xF8);
NW_ASSERT_OFFSET(NCGMInventoryWnd, TabCtrl, 0x104);
NW_ASSERT_SIZE(NCGMInventoryWnd, 0x108);

// An item the GM's quest window counts (+0x180's elements; names ours)
struct NGMQuestCount
{
    INT ItemID;            // +0x00 compared with the quest's required item ids
    INT Count;             // +0x04 the amount the player holds (the console adds the entries)
};

class NCGMQuestWnd : public NCQuestGaraTreeWnd
{
    NW_DECLARE_CLASS(NCGMQuestWnd, NCQuestGaraTreeWnd, 0, 0x102D6570)

    TArray<NGMQuestCount> Counts;         // +0x1A4 (C3, GM only) empty after OnCreate; the console fills it (AddQuestItem)

    NCGMQuestWnd();                                                  // 0x10090300 (the base's, then +0x180 cleared)
    // ~NCGMQuestWnd                                                 // 0x10090360 (Counts, then the base's 0x10084280)

    // the base's overrides it replaces
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100891f0 (NCQuestGaraTreeWnd's as 0 / 1)
    virtual INT OnCreate();                                          // 37 0x1008e9c0
    virtual INT OnDestroy();                                         // 38 0x101566e0 (folded: NCSSQMainEventWnd's)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008c990
    // 68 0x10082870 the base's ClearList with the item counts emptied and the number of quests cleared. ShowWindow
    // (44, 0x100DA310), the tool tip (57, 0x100DC8A0), the mouse move (61, 0x100DB6D0) and the two buttons (69 / 70,
    // 0x100DB4A0 / 0x100DC050) are the base's
    virtual void ClearList();
};
NW_ASSERT_OFFSET(NCGMQuestWnd, Counts, 0x1A4);
NW_ASSERT_SIZE(NCGMQuestWnd, 0x1B0);

// the GM's clan page (UClass "CGMPledgeWnd", 0x964): drawn 17 pixels lower than the player's
class NCGMPledgeWnd : public NCPledgeWnd
{
    NW_DECLARE_CLASS(NCGMPledgeWnd, NCPledgeWnd, 0, 0x102D84A0)

    NCGMPledgeWnd();                                                 // 0x10090460
    // ~NCGMPledgeWnd                                                // 0x100904b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10089280
    virtual INT OnDestroy();                                         // 38 0x100892a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008cc50
};
NW_ASSERT_SIZE(NCGMPledgeWnd, 0x968);

class NCGMActionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMActionWnd, NCWnd, 0, 0x102D50A8)

    NCGMPledgeWnd* PledgeWnd;        // +0xE8 the clan page under the title bar

    NCGMActionWnd();                                                 // 0x10090500
    // ~NCGMActionWnd                                                // 0x10090550

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100905a0
    virtual INT OnDestroy();                                         // 38 0x1008d010
    virtual void ShowWindow();                                       // 48 0x10089320
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ae1c0
};
NW_ASSERT_OFFSET(NCGMActionWnd, PledgeWnd, 0xE8);
NW_ASSERT_SIZE(NCGMActionWnd, 0xEC);

#pragma pack(pop)
