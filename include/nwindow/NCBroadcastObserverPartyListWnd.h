#pragma once
// NCBroadcastObserverPartyListWnd - one team's list of nine player rows in the broadcast observer screen (UClass
// "CBroadcastObserverPartyListWnd", 0xF4; C3 only; NCBroadcastObserverWnd +0xD4 / +0xD8): a column of
// NCBroadcastObserverPartyWnd rows (280 x 70 each) of which the ones with a player on the roster are shown. It
// answers the rows' questions about the roster through its owner.
// Layout and virtuals from the stock constructor 0x10115260, OnCreate 0x10115140, OnDestroy 0x10115080, ShowWindow
// 0x10114F70 and the small methods 0x10114CC0 .. 0x10114FF0.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCBroadcastObserverWnd;
class NCBroadcastObserverPartyWnd;
struct NBroadcastUser;

class NCBroadcastObserverPartyListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastObserverPartyListWnd, NCWnd, 0, 0x10312D58)

    NCBroadcastObserverPartyWnd* Rows[9]; // +0xE8 (name ours) one row per roster slot, 70 pixels apart
    INT Team;                             // +0x10C (name ours) 0 or 1: the team it lists
    NCBroadcastObserverWnd* Owner;        // +0x110 the observer window (NULL: nothing is answered)

    NCBroadcastObserverPartyListWnd(INT InTeam = -1, NCBroadcastObserverWnd* InOwner = NULL); // 0x1012b040 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~NCBroadcastObserverPartyListWnd                              // 0x1012b0d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x1012af10
    virtual INT OnDestroy();                                         // 38 0x1012ae50
    virtual void ShowWindow();                                       // 48 0x1012ad40 NCWnd's, then the rows' visibility

    NBroadcastUser* GetUser(INT Slot);                               // 0x1012aa90 (ret 4) the roster line of a row
    void SetSelectedUser(INT Slot);                                  // 0x1012ab40 (ret 4) the owner follows this row's player
    INT IsSelected(INT Slot);                                        // 0x1012abd0 (ret 4)
    void RefreshShowStatus();                                        // 0x1012ac70 the rows with a player shown, the window as high
    INT IsShowClassOrName();                                         // 0x1012adc0 the owner's PartyShowMode (0 without an owner)
    INT GetTeam();                                                   // 0x1012ab30 (anonymous, name ours; a one-line getter)
};
NW_ASSERT_OFFSET(NCBroadcastObserverPartyListWnd, Rows, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyListWnd, Team, 0x10C);
NW_ASSERT_OFFSET(NCBroadcastObserverPartyListWnd, Owner, 0x110);
NW_ASSERT_SIZE(NCBroadcastObserverPartyListWnd, 0x114);

#pragma pack(pop)
