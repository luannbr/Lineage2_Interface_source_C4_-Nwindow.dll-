#pragma once
// NCPartyWnd - the party (UClass "CPartyWnd", 0x104): a column of eight NCPartyStatusWnd, one
// per member, under a vertical title bar on the left, and a button that switches the buff icons
// of the members on and off. The console hands it the server's party packets (AddMember,
// UpdateMember, DeleteMember, DeleteAll); the window grows and shrinks with the members shown.
//
// Desktop (NCConsole) +0xB0 is the party window; the console's vslots 130..133 (add, update,
// delete, delete all) forward the party packets here.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPartyStatusWnd;
class NCButton;
class NCResizeFrame;

enum { NUM_PARTY_SLOTS = 8 };

class NCPartyWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPartyWnd, NCWnd, 0, 0x10309668)

    UTexture* FrameTopTex;           // +0xE8 "L2UI_CH3.FrameCtrl.smallbar1" left edge, shown while there are members
    UTexture* FrameCenterTex;        // +0xEC "L2UI_CH3.FrameCtrl.smallbar2"
    UTexture* FrameBottomTex;        // +0xF0 "L2UI_CH3.FrameCtrl.smallbar3"
    INT NumMembers;                  // +0xF4
    UBOOL bShown;                    // +0xF8 members listed (set by AddMember, cleared with the last member)
    UBOOL bToggle;                   // +0xFC (name ours) flipped by OnToggle; the stock code does not read it
    NCPartyStatusWnd* Members[NUM_PARTY_SLOTS]; // +0x100 filled from the top, empty = no name
    NCButton* BuffButton;            // +0x120 "party_buffbutton" (NID_PARTY_TOGGLE)
    INT ExtraWidth;                  // +0x124 (new in C4, name ours) how far the right edge widened the window (0 .. 202)
    INT MaxExtraWidth;               // +0x128 (new in C4, name ours) 202, set by each resize
    NCResizeFrame* ResizeFrame;      // +0x12C (new in C4) ID 0xFF0001F3: the right edge that widens the window

    NCPartyWnd();                                                    // 0x10117480

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1010de50
    virtual INT OnCreate();                                          // 37 0x10117530
    virtual INT OnDestroy();                                         // 38 0x101105a0
    virtual void ShowWindow();                                       // 48 0x101109c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101106b0
    virtual void NewVirtual74();  // 74 0x101109d0 (only this client has it; defined in NNewVirtuals.gen.cpp): OnResize(INT Delta), see OnResizeImpl
    virtual INT OnToggle();                                          // 75 0x1010ded0 (name ours)

    // names ours. C4: the console hands the packet's parameters over (C3: AddMember popped them and had UpdateMember for
    // the later updates; C4 has no UpdateMember)
    __declspec(noinline) void UpdateLayout();                        // 0x10110820
    void AddMember(L2ParamStack& Params);                            // 0x10115630 (ret 4)
    // 0x101109d0 (slot 74 of the stock vtable, where the header keeps the placeholder NewVirtual74 that jumps here): the right
    // edge was dragged by Delta, the window grows or shrinks within 0 .. 202 pixels
    INT OnResizeImpl(INT Delta);
    void DeleteMember(INT MemberID, const TCHAR* Name);              // 0x101156a0 by name, the
                                                                     //   id is not read
    void DeleteAll();                                                // 0x101158c0
    // names ours (the console's party packets): the member with that id gets a buff icon (the last argument is passed on to
    // NCPartyStatusWnd::AddBuff, ret 0xC) / loses its buff icons
    void AddMemberBuff(INT MemberID, INT SkillID, INT Unused);       // 0x10115920
    void ClearMemberBuffs(INT MemberID);                             // 0x10115980
};
NW_ASSERT_OFFSET(NCPartyWnd, FrameTopTex, 0xE8);
NW_ASSERT_OFFSET(NCPartyWnd, NumMembers, 0xF4);
NW_ASSERT_OFFSET(NCPartyWnd, bToggle, 0xFC);
NW_ASSERT_OFFSET(NCPartyWnd, Members, 0x100);
NW_ASSERT_OFFSET(NCPartyWnd, BuffButton, 0x120);
NW_ASSERT_OFFSET(NCPartyWnd, ExtraWidth, 0x124);
NW_ASSERT_OFFSET(NCPartyWnd, ResizeFrame, 0x12C);
NW_ASSERT_SIZE(NCPartyWnd, 0x130);

#pragma pack(pop)
