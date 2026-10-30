#pragma once
// NCPledgeWnd - the clan page of the action window (UClass "CPledgeWnd", 0x3B0): crest, name,
// leader, level, castle / hideout, the member list (name, level, on / out; NCListCtrl) and
// nine buttons: invite, title, crest, war, truce, surrender, expel, yield, leave. Only the
// clan leader gets the first seven; only the others get Leave. The console fills it
// (NConsoleWnd::ReceivePledgeInfo, AddPledgeMember...). NCActionWnd +0x9C is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCListCtrl;
class NCButton;

// What the console hands over about the clan (name ours; 0x62C bytes in C2, 0xEC in C1: the three
// texts are 256 characters now; built on the stack by NConsoleWnd::ReceivePledgeInfo). The window
// keeps a copy (Info) but SetPledgeInfo copies only part of it.
// C2 layout: from the stock SetPledgeInfo / UpdatePledgeInfo (the copies field by field) and the
// window's constructor and ClearInfo (the stores into the copy); C1 offsets in brackets.
struct NPledgeInfoBase
{
    TCHAR Name[256];           // +0x000 (C1: 32)
    INT PledgeID;              // +0x200 the crest texture is looked up by it (C1 +0x40)
    INT CrestID;               // +0x204 > 0: the clan has a crest (C1 +0x44)
    TCHAR LeaderName[256];     // +0x208 (C1 +0x48, 32 characters)
    INT Level;                 // +0x408 (C1 +0x88)
    UBOOL bHasCastle;          // +0x40C (C1 +0x8C)
    UBOOL bHasHideout;         // +0x410 (C1 +0x90)
    INT Unknown94;             // +0x414 (C1 +0x94)
    INT Unknown98;             // +0x418 (C1 +0x98)
    TCHAR StatusText[256];     // +0x41C "In War", "In Siege", "Dispersing"... (not copied) (C1 +0x9C, 32)
    INT Status;                // +0x61C index the console picked StatusText by (C1 +0xDC)
    INT UnknownE0;             // +0x620 (not copied)
    INT UnknownE4;             // +0x624 (not copied)
    INT UnknownE8;             // +0x628 (C1 +0xE8)
};

// the record with the constructor the console's slots inline (the page keeps NPledgeInfoBase: its constructor runs the
// stock's NPledgeInfoExt::Clear, 0x1004ce80, which also clears the three dwords of the page after the record)
struct NPledgeInfo : public NPledgeInfoBase
{
    // inlined in the console's slots 75, 76 and 203 (UnknownE8 is left)
    NPledgeInfo()
    {
        Name[0] = 0;
        LeaderName[0] = 0;
        StatusText[0] = 0;
        PledgeID = 0;
        CrestID = 0;
        UnknownE4 = 0;
        Level = 0;
        bHasCastle = 0;
        bHasHideout = 0;
        Unknown94 = 0;
        Unknown98 = 0;
        Status = 0;
        UnknownE0 = 0;
    }
};
NW_ASSERT_OFFSET(NPledgeInfoBase, PledgeID, 0x200);
NW_ASSERT_OFFSET(NPledgeInfoBase, LeaderName, 0x208);
NW_ASSERT_OFFSET(NPledgeInfoBase, Level, 0x408);
NW_ASSERT_OFFSET(NPledgeInfoBase, StatusText, 0x41C);
NW_ASSERT_OFFSET(NPledgeInfoBase, Status, 0x61C);
NW_ASSERT_SIZE(NPledgeInfoBase, 0x62C);
NW_ASSERT_SIZE(NPledgeInfo, 0x62C);

// A member as the console hands it over (name ours; 0x214 bytes, built on the stack by
// NConsoleWnd::AddPledgeMember)
struct NPledgeMember
{
    TCHAR Name[256];           // +0x000
    INT Level;                 // +0x200
    INT ClassID;               // +0x204 (-1)
    INT Unknown208;            // +0x208 (-1)
    INT Unknown20C;            // +0x20C (-1)
    UBOOL bOnline;             // +0x210 C2: the member's id while it is online, 0 when not (the row keeps it as its first
                               //   param: NCPledgeWnd::OnNewButton2 sends it as the member of the privileges request)

    // inlined in the console's slots 78, 79 and 205
    NPledgeMember()
    {
        Name[0] = 0;
        Level = 0;
        ClassID = -1;
        Unknown208 = -1;
        Unknown20C = -1;
        bOnline = 0;
    }
    BYTE _PadC3[52];   // this client grew the struct (db/struct_sizes.csv)
};
NW_ASSERT_OFFSET(NPledgeMember, Level, 0x200);
NW_ASSERT_OFFSET(NPledgeMember, bOnline, 0x210);
NW_ASSERT_SIZE(NPledgeMember, 0x248);

// NWM_COMMAND ids of the buttons (names ours)
enum
{
    NID_PLEDGE_CREST = 0xF00C,
    NID_PLEDGE_WAR = 0xF00D,
    NID_PLEDGE_TRUCE = 0xF00E,
    NID_PLEDGE_SURRENDER = 0xF010,
    NID_PLEDGE_INVITE = 0xF011,
    NID_PLEDGE_YIELD = 0xF012,
    NID_PLEDGE_TITLE = 0xF013,
    NID_PLEDGE_EXPEL = 0xF014,
    NID_PLEDGE_LEAVE = 0xF015,
    NID_PLEDGE_NEW1 = 0xF019,         // new in C2 (text 666)
    NID_PLEDGE_NEW2 = 0xF018,         // new in C2 (text 667)
    NID_PLEDGE_NEW3 = 0xF01A,         // new in C3 (text 842, name ours)
    NID_PLEDGE_NEW4 = 0xF01B,         // new in C3 (text 843, name ours)
    NID_PLEDGE_NEW5 = 0xF01C,         // new in C4 (text 1207, name ours): OnBypass
    NID_PLEDGE_NEW6 = 0xF01D,         // new in C4 (text 387, name ours): OnBoard
};

class NCPledgeWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPledgeWnd, NCWnd, 0, 0x102CE240)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.BloodHoodWnd.BloodHood_Back"
    UTexture* CrestTex;              // +0xEC (NULL) NCGMPledgeWnd: the crest found by each paint   // C2: offset derived, no evidence
    NPledgeInfoBase Info;            // +0xF0 (0x62C bytes in C2; C1 +0x9C, 0xEC bytes)
    INT Unknown6FC;                  // +0x71C C3: SetPledgeInfo / UpdatePledgeInfo copy the console record's +0x62C here (name ours):
                                     //   the castle's id (OnPaint: GetCastleName(it) when bHasCastle)
    INT Unknown700;                  // +0x720 the record's +0x630 (name ours): the hideout's id (the same, when bHasHideout)
    INT Unknown704;                  // +0x724 the record's +0x634 (name ours): 0 picks sysstring 894 as the status text
    NCListCtrl* MemberList;          // +0x728 four columns: name, level, class (an icon), online (an icon)
    // the buttons, in the order OnCreate (0x1004ef80) creates them; fifteen in C4 (C3: thirteen). Offsets and ids from the
    // C4 OnCreate; the C3 header named +0x748 Yield and +0x75C Leave, the ids say the other way round
    NCButton* InviteButton;          // +0x72C id 0xF011 text 330
    NCButton* TitleButton;           // +0x730 id 0xF013 text 331
    NCButton* Button714;             // +0x734 id 0xF01A text 842 (new in C3; name ours) -> vslot 86 (OnRemoveTitle)
    NCButton* WarButton;             // +0x738 id 0xF00D text 1205
    NCButton* TruceButton;           // +0x73C id 0xF00E text 1206
    NCButton* CrestButton;           // +0x740 id 0xF00C text 332
    NCButton* ExpelButton;           // +0x744 id 0xF014 text 338
    NCButton* LeaveButton;           // +0x748 id 0xF015 text 337 -> vslot 81 (OnWithdraw); the only one a member that does not lead can use
    NCButton* Button72C;             // +0x74C id 0xF01B text 843 (new in C3; name ours) -> vslot 87 (OnRemoveCrest)
    NCButton* NewButton6F4;          // +0x750 id 0xF019 text 666 (OnNewButton1)
    NCButton* NewButton6F8;          // +0x754 id 0xF018 text 667 (OnNewButton2)
    NCButton* SurrenderButton;       // +0x758 id 0xF010 text 1208 -> vslot 78 (OnSurrender)
    NCButton* YieldButton;           // +0x75C id 0xF012 text 1209 -> vslot 80 (OnSurrenderPersonally)
    NCButton* Button760;             // +0x760 id 0xF01C text 1207 (new in C4; name ours) -> vslot 88 (the bypass command 100)
    NCButton* Button764;             // +0x764 id 0xF01D text 387 (new in C4; name ours) -> vslot 89 (opens the community window's fourth tab)
    BYTE _Unknown768[0x968 - 0x768]; // +0x768 not used by the stock code of this class

    NCPledgeWnd();                                                   // 0x100508d0
    // ~NCPledgeWnd                                                  // 0x10041020 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1004c3f0
    virtual INT OnCreate();                                          // 37 0x1004ef80
    virtual INT OnDestroy();                                         // 38 0x1004d900
    virtual void ShowWindow();                                       // 48 0x1004c630
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1004d4a0
    // the buttons (names ours): dialogs NCDialogBox answers by ID, or messages
    virtual INT OnInvite();                                          // 74 0x1004cbb0
    virtual INT OnSetCrest();                                        // 75 0x1004e310
    virtual INT OnDeclareWar();                                      // 76 0x1004e3b0
    virtual INT OnEndWar();                                          // 77 0x1004e450
    virtual INT OnSurrender();                                       // 78 0x1004e4f0
    virtual INT OnExpel();                                           // 79 0x1004e590
    virtual INT OnSurrenderPersonally();                             // 80 0x1004e680
    virtual INT OnWithdraw();                                        // 81 0x1004e710
    virtual INT OnGiveTitle();                                       // 82 0x1004e0f0
    virtual INT OnNewButton1();                                      // 83 0x1004e7f0 (C2 only; name ours) the button 0xF019 (NewButton6F4): the player's own
                                                                     //   privileges (the empower window, view mode)
    virtual INT OnNewButton2();                                      // 84 0x1004e8b0 (C2 only; name ours) the button 0xF018 (NewButton6F8): the selected
                                                                     //   member's privileges (the empower window, edit mode; the leader's)
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam) { return 1; } // 85 0x100e6a50 (folded)
    virtual INT OnRemoveTitle();                                     // 86 0x1004e1c0 (new in C3; name ours) take the targeted
                                                                     //   player's title away (confirmation dialog 0x48)
    virtual INT OnRemoveCrest();                                     // 87 0x1004e280 (new in C3; name ours) delete the clan's
    virtual void NewVirtual88();  // 88 0x1004cc60 (only this client has it; defined in NNewVirtuals.gen.cpp): see OnBypass
    virtual void NewVirtual89();  // 89 0x1004ccd0 (only this client has it; defined in NNewVirtuals.gen.cpp): see OnBoard
                                                                     //   crest (confirmation dialog 0x49)

    // the console's calls (names ours)
    void SetPledgeInfo(NPledgeInfo* InInfo);                         // 0x1004c9d0
    void UpdatePledgeInfo(NPledgeInfo* InInfo);                      // 0x1004cab0
    __declspec(noinline) void ClearMembers();                        // 0x1004cb40
    __declspec(noinline) void ClearInfo();                           // 0x1004cb50
    void Reset();                                                    // 0x1004dc40
    void AddMember(NPledgeMember* Member);                           // 0x1004dc50
    void UpdateMember(NPledgeMember* Member);                        // 0x1004df30
    void DeleteMember(const TCHAR* Name);                            // 0x1004fe70
    // the two new buttons' commands (slots 88 and 89, whose placeholders NewVirtual88 / NewVirtual89 jump to the stock
    // addresses where these two are hooked; the window procedure calls the slots)
    INT OnBypass();                                                  // 0x1004cc60 (name ours) the bypass user command 100
    INT OnBoard();                                                   // 0x1004ccd0 (name ours) the community window, its fourth tab
};
NW_ASSERT_OFFSET(NCPledgeWnd, Info, 0xF0);
NW_ASSERT_OFFSET(NCPledgeWnd, Unknown6FC, 0x71C);
NW_ASSERT_OFFSET(NCPledgeWnd, MemberList, 0x728);
NW_ASSERT_OFFSET(NCPledgeWnd, CrestButton, 0x740);
NW_ASSERT_OFFSET(NCPledgeWnd, NewButton6F8, 0x754);
NW_ASSERT_OFFSET(NCPledgeWnd, LeaveButton, 0x748);
NW_ASSERT_OFFSET(NCPledgeWnd, YieldButton, 0x75C);
NW_ASSERT_OFFSET(NCPledgeWnd, Button764, 0x764);
NW_ASSERT_SIZE(NCPledgeWnd, 0x968);

#pragma pack(pop)