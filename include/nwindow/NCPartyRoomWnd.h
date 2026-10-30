#pragma once
// NCPartyRoomWnd - the window of the party room the player is in (UClass "CPartyRoomWnd", 0x308; new in C3): the
// room's number, title, place, loot rule, level range and members (NCMatchPartyListCtrl: name, class, level range,
// place, role), a chat line (the room's own chat, type 14) and the buttons Manage and Ban (the owner's), Invite and
// Close. The console fills it (SetRoomInfo, SetMembers, UpdateMember) and routes the room's chat to it (AddChatLine).
//
// Desktop (NCConsole) +0x148 is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCListBox;
class NCEditBox;
class NCMatchPartyListCtrl;
class NCPartyRoomFrameCtrl;
class NCButton;

// NWM_COMMAND ids of the controls (names ours)
enum
{
    NID_PARTYROOM_FRAME = 0xF7,
    NID_PARTYROOM_CHAT = 0xFFA0F020,         // the chat list
    NID_PARTYROOM_EDIT = 0xFFA0F021,         // the chat line: Enter sends it
    NID_PARTYROOM_MANAGE = 0xFFA0F023,       // vslot 68
    NID_PARTYROOM_BAN = 0xFFA0F024,          // vslot 69
    NID_PARTYROOM_INVITE = 0xFFA0F025,       // vslot 70
    NID_PARTYROOM_CLOSE = 0xFFA0F026,        // vslot 71
};

// who the player is in the room (Mode, set by SetMembers / UpdateMember; names ours)
enum { PARTYROOM_Member = 0, PARTYROOM_Owner = 1, PARTYROOM_Other = 2 };

class NCPartyRoomWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPartyRoomWnd, NCWnd, 0, 0x102E6E90)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.PartyMatchWnd.PartyMatch2_Back"
    NCListBox* ChatList;             // +0xEC (names ours) an NCTextListBox in C4 (C3: an NCPetitionListBox): 40 lines, 4 visible
    NCEditBox* ChatEdit;             // +0xF0
    NCMatchPartyListCtrl* MemberList; // +0xF4 five columns
    NCButton* ManageButton;          // +0xF8 enabled for the owner
    NCButton* BanButton;             // +0xFC enabled for the owner
    NCButton* InviteButton;          // +0x100 enabled for the owner
    NCButton* CloseButton;           // +0x104
    INT RoomID;                      // +0x108
    TCHAR Title[256];                // +0x10C
    INT Location;                    // +0x30C the place (1 .. 13)
    INT LootRule;                    // +0x310 0 .. 4
    INT MinLevel;                    // +0x314
    INT MaxLevel;                    // +0x318
    INT MaxMembers;                  // +0x31C
    INT Mode;                        // +0x320 PARTYROOM_*: what the player is in the room (the buttons follow it)
    NCPartyRoomFrameCtrl* RoomFrame; // +0x324 id 0xF7, the title bar   // C2: offset derived, no evidence

    NCPartyRoomWnd();                                                // 0x100b5520
    // ~NCPartyRoomWnd                                               // 0x100b5570

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b31c0
    virtual INT OnCreate();                                          // 37 0x100b55c0
    virtual INT OnDestroy();                                         // 38 0x100b32a0
    virtual void ShowWindow();                                       // 48 0x100b71f0 (folded)
    virtual void Minimize();                                         // 54 0x100b1630 (NCMatchWnd's: folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b3480
    // the buttons (names ours)
    virtual INT OnManageBtnClick();                                  // 74 0x100b18c0
    virtual INT OnBanBtnClick();                                     // 75 0x100b19b0
    virtual INT OnInviteBtnClick();                                  // 76 0x100b1a70
    virtual INT OnCloseBtnClick();                                   // 77 0x100b3940

    // the console's calls (names ours)
    void SetRoomInfo(L2ParamStack& Params);                          // 0x100b1c10 (the console's slot 376)
    void SetMembers(L2ParamStack& Params);                           // 0x100a6ea0 (C1 only: no C2 match) (slot 377)
    void UpdateMember(L2ParamStack& Params);                         // 0x100b3eb0 (slot 379)
    void AddChatLine(const TCHAR* Text, DWORD Color);                // 0x100b1f70 (ret 8: NCConsole::Say's)

    // inlined here and there (names ours)
    void RequestExit();                                              // 0x100b1b40 asks to dismiss (owner) or leave
    INT OnChatEnter(LPARAM lParam);                                  // 0x100b1fa0 (ret 4)
};
NW_ASSERT_OFFSET(NCPartyRoomWnd, ChatList, 0xEC);
NW_ASSERT_OFFSET(NCPartyRoomWnd, MemberList, 0xF4);
NW_ASSERT_OFFSET(NCPartyRoomWnd, CloseButton, 0x104);
NW_ASSERT_OFFSET(NCPartyRoomWnd, RoomID, 0x108);
NW_ASSERT_OFFSET(NCPartyRoomWnd, Title, 0x10C);
NW_ASSERT_OFFSET(NCPartyRoomWnd, Location, 0x30C);
NW_ASSERT_OFFSET(NCPartyRoomWnd, MaxMembers, 0x31C);
NW_ASSERT_OFFSET(NCPartyRoomWnd, Mode, 0x320);
NW_ASSERT_OFFSET(NCPartyRoomWnd, RoomFrame, 0x324);
NW_ASSERT_SIZE(NCPartyRoomWnd, 0x328);

#pragma pack(pop)
