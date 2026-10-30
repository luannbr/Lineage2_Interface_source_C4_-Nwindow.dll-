#pragma once
// RemoveFriendDialog - the box that asks before a contact is removed (UClass "RemoveFriendDialog", 0x230; C3 only, made by
// MsnWnd::OnCreate with the window and the protocol object): a question, a check box ("also block this contact") and
// OK / Cancel. OK removes the contact selected in the window's tree (its account is read from the tree item) and closes
// the box, or, with nothing selected, the window shows a message instead; showing the box clears the check box.

#include "nwindow/NMsnDlgCommon.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NMsnBuddy.h"
#include "nwindow/NMsnLink.h"
#pragma pack(push, 4)

enum
{
    NID_REMOVEFRIEND_OK = 0xA33,   // names ours
    NID_REMOVEFRIEND_CANCEL = 0xA34,
};

class RemoveFriendDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(RemoveFriendDialog, NCDialogBox, 0, 0x1031D940)

    MsnWnd* Wnd;                     // +0x1F4 the messenger window (constructor argument 1)
    NMsnLink* Msn;                   // +0x1F8 the protocol object (constructor argument 2)
    NMsnBuddy Buddy;                 // +0x1FC a contact record, made and destroyed with the box (nothing in this class uses it)
    NCCheckBox* BlockCheck;          // +0x240 12 high at (13, 63): the contact is blocked too (SysString-less text: systemmsg 1316)
    NCButton* OKButton;              // +0x244 id 0xA33, 76 x 23 at (51, 112)
    NCButton* CancelButton;          // +0x248 id 0xA34, at (131, 112)
    UTexture* BackTex;               // +0x24C "L2UI_CH3.Msn.msn_optionback4"

    RemoveFriendDialog(MsnWnd* InWnd = NULL, NMsnLink* InMsn = NULL); // 0x101485f0
    // ~RemoveFriendDialog                                           // 0x101486a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10146b40
    virtual INT OnCreate();                                          // 37 0x10146bb0
    virtual INT OnDestroy();                                         // 38 0x101455f0
    virtual void ShowWindow();                                       // 48 0x101457e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10145690

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x10145800 the selected contact removed (and blocked), the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(RemoveFriendDialog, Wnd, 0x1F4);
NW_ASSERT_OFFSET(RemoveFriendDialog, Msn, 0x1F8);
NW_ASSERT_OFFSET(RemoveFriendDialog, Buddy, 0x1FC);
NW_ASSERT_OFFSET(RemoveFriendDialog, BlockCheck, 0x240);
NW_ASSERT_OFFSET(RemoveFriendDialog, OKButton, 0x244);
NW_ASSERT_OFFSET(RemoveFriendDialog, CancelButton, 0x248);
NW_ASSERT_OFFSET(RemoveFriendDialog, BackTex, 0x24C);
NW_ASSERT_SIZE(RemoveFriendDialog, 0x250);

#pragma pack(pop)
