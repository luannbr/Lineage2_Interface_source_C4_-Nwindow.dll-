#pragma once
// InviteWnd - "invite a friend to this conversation" (UClass "InviteWnd", 0xE0; C3 only, the messenger). A small window
// of ChatWnd's with a title bar, the list of the friends (InviteBox) and OK / Cancel; OK (or a double click on a friend)
// sends the SwitchboardSession's invitation for the selected one (CAL) and hides the window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class InviteBox;
class NCButton;

class InviteWnd : public NCWnd
{
    NW_DECLARE_CLASS(InviteWnd, NCWnd, 0, 0x1031C478)

    void* Msn;                       // +0xE8 (name ours) constructor argument 1 (ChatWnd +0xCC): the Msn object (its contacts)
    void* Switchboard;               // +0xEC (name ours) constructor argument 2 (ChatWnd +0xD0): the conversation's
                                     //   SwitchboardSession, the invitation goes through it (0x1012E6B0)
    InviteBox* FriendList;           // +0xF0 300 lines, 10 shown, 9 / 47 at 322 x 177 (LineGap 5)
    NCButton* OKButton;              // +0xF4 id 0xA0F, SysString 140
    NCButton* CancelButton;          // +0xF8 id 0xA10, SysString 141
    UTexture* BackTex;               // +0xFC "L2UI_CH3.Msn.msn_optionback"

    InviteWnd(void* InMsn = NULL, void* InSwitchboard = NULL);                     // 0x10142150 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~InviteWnd                                                    // 0x101421d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10141180
    virtual INT OnCreate();                                          // 37 0x10142220
    virtual INT OnDestroy();                                         // 38 0x1013ed70
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013ee10

    // names ours
    INT OnInvite();                                                  // 0x1013eef0 the selected friend is invited, the window hidden
    INT OnDblClick(WPARAM wParam, LPARAM lParam);                    // 0x10140170 (ret 8) the same for a double click
    void FillFriends();                                              // 0x10143960 the list emptied and filled with the friends not yet in the conversation
};
NW_ASSERT_OFFSET(InviteWnd, Switchboard, 0xEC);
NW_ASSERT_OFFSET(InviteWnd, FriendList, 0xF0);
NW_ASSERT_OFFSET(InviteWnd, BackTex, 0xFC);
NW_ASSERT_SIZE(InviteWnd, 0x100);

#pragma pack(pop)
