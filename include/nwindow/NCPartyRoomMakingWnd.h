#pragma once
// NCPartyRoomMakingWnd - the window that opens or edits a party room (UClass "CPartyRoomMakingWnd", 0xE8; new in
// C3): the title, how many members (2 to 9), the level range, OK and Cancel. OK asks the server to open the room
// (or, with the number of a room the player owns, to change it) and closes the window.
//
// Opened by NCMatchWnd (a new room) and by NCPartyRoomWnd's Manage button (the room's own data).
//
// Desktop (NCConsole) +0x144 is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCEditBox;
class NCComboBox;
class NCButton;

// NWM_COMMAND ids of the controls (names ours)
enum
{
    NID_ROOMMAKING_TITLE = 0xFF0FF021,
    NID_ROOMMAKING_MEMBERS = 0xFF0FF022,
    NID_ROOMMAKING_MINLEVEL = 0xFF0FF023,
    NID_ROOMMAKING_MAXLEVEL = 0xFF0FF024,
    NID_ROOMMAKING_OK = 0xFF0FF026,          // vslot 68
    NID_ROOMMAKING_CANCEL = 0xFF0FF027,      // vslot 69
};

class NCPartyRoomMakingWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPartyRoomMakingWnd, NCWnd, 0, 0x102E5F00)

    NCEditBox* TitleEdit;            // +0xE8 id 0xFF0FF021 (names ours)
    NCComboBox* MembersCombo;        // +0xEC id 0xFF0FF022, "2" .. "9": the selected index + 2 members
    NCEditBox* MinLevelEdit;         // +0xF0 id 0xFF0FF023 (a number, Type 3)
    NCEditBox* MaxLevelEdit;         // +0xF4 id 0xFF0FF024
    NCButton* OKButton;              // +0xF8 id 0xFF0FF026
    NCButton* CancelButton;          // +0xFC id 0xFF0FF027
    UTexture* BackTex;               // +0x100 "L2UI_CH3.PartyMatchWnd.PartyMatch3_Back"
    INT RoomID;                      // +0x104 0: a new room, else the room being changed (SetData)

    NCPartyRoomMakingWnd();                                          // 0x100b53b0
    // ~NCPartyRoomMakingWnd                                         // 0x100b5400

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b16b0
    virtual INT OnCreate();                                          // 37 0x100b39c0
    virtual INT OnDestroy();                                         // 38 0x100b2af0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b2c70
    // the buttons (names ours)
    virtual INT OnOkBtnClick();                                      // 74 0x100b2f30
    virtual INT OnCancelBtnClick();                                  // 75 0x100b1830

    // the window's contents for a room (the sixth argument is not read): the room's number, its title, the
    // selected index of the members box (members - 2), the level range as numbers (name ours)
    void SetData(INT InRoomID, const TCHAR* Title, INT MembersIndex, INT MinLevel, INT MaxLevel, INT Unused); // 0x100b1730 (ret 0x18)
};
NW_ASSERT_OFFSET(NCPartyRoomMakingWnd, TitleEdit, 0xE8);
NW_ASSERT_OFFSET(NCPartyRoomMakingWnd, CancelButton, 0xFC);
NW_ASSERT_OFFSET(NCPartyRoomMakingWnd, BackTex, 0x100);
NW_ASSERT_OFFSET(NCPartyRoomMakingWnd, RoomID, 0x104);
NW_ASSERT_SIZE(NCPartyRoomMakingWnd, 0x108);

#pragma pack(pop)
