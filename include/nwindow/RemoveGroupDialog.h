#pragma once
// RemoveGroupDialog - the box that asks which contact group to remove (UClass "RemoveGroupDialog", 0x1EC; C3 only, made by
// MsnWnd::OnCreate with the window and the protocol object): a combo box listing the groups (filled when the box is
// shown) and OK / Cancel. OK removes the chosen group, unless it still has contacts (then the window shows a message and
// the box stays), and closes the box.

#include "nwindow/NMsnDlgCommon.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NMsnLink.h"
#pragma pack(push, 4)

enum
{
    NID_REMOVEGROUP_OK = 0xA2B,    // names ours
    NID_REMOVEGROUP_CANCEL = 0xA2C,
};

class RemoveGroupDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(RemoveGroupDialog, NCDialogBox, 0, 0x103202D0)

    MsnWnd* Wnd;                     // +0x1F4 the messenger window (constructor argument 1)
    NMsnLink* Msn;                   // +0x1F8 the protocol object (constructor argument 2)
    NCComboBox* GroupCombo;          // +0x1FC the groups, 156 x 17 at (50, 48) (child style NWS_DLG_COMBO)
    NCButton* OKButton;              // +0x200 id 0xA2B, 76 x 23 at (51, 75)
    NCButton* CancelButton;          // +0x204 id 0xA2C, at (131, 75)
    UTexture* BackTex;               // +0x208 "L2UI_CH3.dialog.system_back"

    RemoveGroupDialog(MsnWnd* InWnd = NULL, NMsnLink* InMsn = NULL); // 0x10148a80
    // ~RemoveGroupDialog                                            // 0x10148b00

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10147d20
    virtual INT OnCreate();                                          // 37 0x10147d90
    virtual INT OnDestroy();                                         // 38 0x10146310
    virtual void ShowWindow();                                       // 48 0x101483b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101463b0

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x101464b0 the chosen group removed, the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(RemoveGroupDialog, Wnd, 0x1F4);
NW_ASSERT_OFFSET(RemoveGroupDialog, Msn, 0x1F8);
NW_ASSERT_OFFSET(RemoveGroupDialog, GroupCombo, 0x1FC);
NW_ASSERT_OFFSET(RemoveGroupDialog, OKButton, 0x200);
NW_ASSERT_OFFSET(RemoveGroupDialog, CancelButton, 0x204);
NW_ASSERT_OFFSET(RemoveGroupDialog, BackTex, 0x208);
NW_ASSERT_SIZE(RemoveGroupDialog, 0x20C);

#pragma pack(pop)
