#pragma once
// NCGameOptionWnd - the game page of the options window (UClass "CGameOptionWnd", 0x148): language
// (native or English), which names show over characters (player, monsters, other PCs - and among those
// clan, party, general), camera tracking, the chat input options, transparency and arrow modes, the colored
// cursor, the zone name banner, the system message window (and its damage and item lines), a button that puts
// every window back in place, party loot and the game pad. The settings live in Core globals (GL2Show*,
// GIsEnterChatting...) and in Option.ini [Game]. C4 layout from OnCreate (0x100CA4B0) and the stock Apply /
// LoadSettings / OnDestroy: the screen shot quality box of C3 is gone and five check boxes are new.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCComboBox;
class NCButton;
class NCCheckBox;

// NameBoxes[] (names ours): "Other PCs" switches the last three
enum
{
    NAMEBOX_Player,
    NAMEBOX_Monster,
    NAMEBOX_OtherPC,
    NAMEBOX_Clan,
    NAMEBOX_Party,
    NAMEBOX_General,
    NAMEBOX_MAX
};

// NWM_COMMAND ids of the page's controls
enum
{
    NID_GAMEOPT_LANGUAGE = 0x1F15,
    NID_GAMEOPT_COLORCURSOR = 0x1F0F,                 // new in C4 (name ours)
    NID_GAMEOPT_PLAYER = (INT)0xF0F01459,
    NID_GAMEOPT_MONSTER = (INT)0xF0F01460,
    NID_GAMEOPT_OTHERPC = (INT)0xF0F01461,
    NID_GAMEOPT_CLAN = (INT)0xF0F01462,
    NID_GAMEOPT_PARTY = (INT)0xF0F01463,
    NID_GAMEOPT_GENERAL = (INT)0xF0F01464,
    NID_GAMEOPT_TRACKING = (INT)0xF0F01470,
    NID_GAMEOPT_ENTERCHAT = (INT)0xF0F01471,
    NID_GAMEOPT_OLDCHAT = (INT)0xF0F01472,
    NID_GAMEOPT_GAMEPAD = (INT)0xF0F01473,
    NID_GAMEOPT_KEYCRYPT = (INT)0xF0F01474,   // new in C2
    NID_GAMEOPT_TRANSPARENCY = (INT)0xF0F01475,   // new in C3 (name ours)
    NID_GAMEOPT_ARROW = (INT)0xF0F01476,          // new in C3 (name ours)
    NID_GAMEOPT_ZONETITLE = (INT)0xF0F01478,      // new in C4 (name ours)
    NID_GAMEOPT_SYSMSG = (INT)0xF0F01479,         // new in C4 (name ours)
    NID_GAMEOPT_SYSMSGDAMAGE = (INT)0xF0F01480,   // new in C4 (name ours)
    NID_GAMEOPT_SYSMSGITEM = (INT)0xF0F01481,     // new in C4 (name ours)
    NID_GAMEOPT_RESET = (INT)0xFFFF1400,
    NID_GAMEOPT_PARTYLOOT = (INT)0xFFFF1401,
};

class NCGameOptionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGameOptionWnd, NCWnd, 0, 0x102ED228)

    NCComboBox* LanguageCombo;             // +0xE8 0 native, 1 English
    NCButton* ResetButton;                 // +0xEC "Initialize"
    NCWnd* Unused9C[3];                    // +0xF0 destroyed, never created (the constructor clears them)
    NCCheckBox* NameBoxes[NAMEBOX_MAX];    // +0xFC
    NCCheckBox* EnterChatBox;              // +0x114 GIsEnterChatting
    NCCheckBox* OldChatBox;                // +0x118 GIsOldChat
    NCCheckBox* TrackingBox;               // +0x11C GL2UseAutoTrackingPawn
    NCCheckBox* TransparencyBox;           // +0x120 GIsTransparencyMode, Option.ini [Game] TransparencyMode
    NCCheckBox* ArrowBox;                  // +0x124 GIsArrowMode, [Game] ArrowMode
    NCCheckBox* ColorCursorBox;            // +0x128 (new in C4, name ours) GL2UseColorCursor ([Video] UseColorCursor; sysstring 305)
    NCCheckBox* ZoneTitleBox;              // +0x12C (new in C4, name ours) GL2ShowZoneTitle (sysstring 1196)
    NCCheckBox* SystemMsgBox;              // +0x130 (new in C4, name ours) GL2SystemMsgWnd: shows the chat window's system list
                                           //   (sysstring 1197); it enables the next two
    NCCheckBox* SystemMsgDamageBox;        // +0x134 (new in C4, name ours) GL2SystemMsgWndDamage (sysstring 357)
    NCCheckBox* SystemMsgItemBox;          // +0x138 (new in C4, name ours) GL2SystemMsgWndExpendableItem (sysstring 796)
    NCComboBox* PartyLootCombo;            // +0x13C GL2PartyLooting: 0 finders keepers, 1 random ...
    NCCheckBox* GamePadBox;                // +0x140 only with a game pad (GL2CanUseJoystick)
    NCCheckBox* KeyCryptBox;               // +0x144 the nProtect key encryption at startup

    NCGameOptionWnd();                                               // 0x100cb430
    // ~NCGameOptionWnd                                              // 0x100cb490

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100c4970
    virtual INT OnCreate();                                          // 37 0x100ca4b0
    virtual INT OnDestroy();                                         // 38 0x100c7a70
    virtual void ShowWindow();                                       // 48 0x100c4a30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100c77f0
    // names ours
    virtual void NewVirtual74();  // 74 0x100c8c40 OnSystemMsgClicked (only this client has it; defined in NNewVirtuals.gen.cpp; hooked through a stand-in class in NCGameOptionWnd.cpp)
    virtual INT OnOtherPCClicked();                                  // 75 0x100c8c90
    virtual INT OnReset();                                           // 76 0x100c4a10

    // names ours
    void LoadSettings();                                             // 0x100c7f60
    void Apply();                                                    // 0x100c8530
    void ClearItems();                                               // 0x100c7e50 (C2 only; before InitItems)
    void InitItems();                                                // 0x100c97d0
};
NW_ASSERT_OFFSET(NCGameOptionWnd, LanguageCombo, 0xE8);
NW_ASSERT_OFFSET(NCGameOptionWnd, Unused9C, 0xF0);
NW_ASSERT_OFFSET(NCGameOptionWnd, NameBoxes, 0xFC);
NW_ASSERT_OFFSET(NCGameOptionWnd, EnterChatBox, 0x114);
NW_ASSERT_OFFSET(NCGameOptionWnd, ColorCursorBox, 0x128);
NW_ASSERT_OFFSET(NCGameOptionWnd, SystemMsgBox, 0x130);
NW_ASSERT_OFFSET(NCGameOptionWnd, PartyLootCombo, 0x13C);
NW_ASSERT_OFFSET(NCGameOptionWnd, GamePadBox, 0x140);
NW_ASSERT_OFFSET(NCGameOptionWnd, KeyCryptBox, 0x144);
NW_ASSERT_SIZE(NCGameOptionWnd, 0x148);

#pragma pack(pop)
