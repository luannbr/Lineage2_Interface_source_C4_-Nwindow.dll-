#pragma once
// NCBroadcastObserverPartyWnd - one player's row in a team's party list of the broadcast observer screen (UClass
// "CBroadcastObserverPartyWnd", 0x114; C3 only; NCBroadcastObserverPartyListWnd +0xC8..): a 280 x 70 banner (three
// pieces, a darker one for a dead player), the team's emblem, the class or name of the player and his CP / HP / MP
// bars. A click passes to the parent list and selects the player (the owner follows him).
// Layout and virtuals from the stock constructor 0x10115F20, OnCreate 0x10115360, OnDestroy 0x101155A0, the mouse
// handlers 0x10115620 .. 0x10115780, GetUser 0x10115820, IsSelected 0x101158C0 and OnPaint 0x10115960.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCBroadcastObserverPartyListWnd;
struct NBroadcastUser;

class NCBroadcastObserverPartyWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastObserverPartyWnd, NCWnd, 0, 0x10313290)

    UTexture* BackTex[3];            // +0xE8 "L2UI_CH3.BroadcastObs.br_party2_back1" .. "3": the banner (names ours)
    UTexture* DeadBackTex[3];        // +0xF4 "br_dead2_back1" .. "3": the banner of a dead player
    UTexture* HpBarTex;              // +0x100 "br_bar2_hp"
    UTexture* MpBarTex;              // +0x104 "br_bar2_mp"
    UTexture* CpBarTex;              // +0x108 "br_bar2_cp"
    UTexture* HpBackTex;             // +0x10C "br_bar2back_hp"
    UTexture* MpBackTex;             // +0x110 "br_bar2back_mp"
    UTexture* CpBackTex;             // +0x114 "br_bar2back_cp"
    UTexture* TeamTex;               // +0x118 "br_party1_<slot+1>" (first team) / "br_party2_<slot+1>" (second): the emblem
    UTexture* DeadTeamTex;           // +0x11C "br_party_X": the emblem of a dead player
    UTexture* SelectTex[3];          // +0x120 "br_partyon_back1" .. "3": the overlay of the selected row
    INT Slot;                        // +0x12C the roster slot (0..8)
    NCBroadcastObserverPartyListWnd* ParentList; // +0x130 the list (and, through it, the owner)

    NCBroadcastObserverPartyWnd(NCBroadcastObserverPartyListWnd* InParent = NULL, INT InSlot = 0); // 0x1012bd20 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~NCBroadcastObserverPartyWnd                                  // 0x1012be20

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x1012b140 (guard: NCBroadcastObserverWnd::OnCreate)
    virtual INT OnDestroy();                                         // 38 0x1012b3a0 (guard: NCBroadcastObserverWnd::OnDestroy)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1012b760 (guard: NCBroadcastObserverWnd::OnPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x1012b580
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1012b420
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x1012b4e0

    NBroadcastUser* GetUser();                                       // 0x1012b620 the row's roster line (the list's)
    INT IsSelected();                                                // 0x1012b6c0
};
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, DeadBackTex, 0xF4);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, HpBarTex, 0x100);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, TeamTex, 0x118);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, SelectTex, 0x120);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, Slot, 0x12C);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyWnd, ParentList, 0x130);
NW_ASSERT_SIZE(NCBroadcastObserverPartyWnd, 0x134);

#pragma pack(pop)
