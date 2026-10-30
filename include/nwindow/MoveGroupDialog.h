#pragma once
// MoveGroupDialog - the box that asks to which contact group a contact moves (UClass "MoveGroupDialog", 0x22C; C3 only,
// made by MsnWnd::OnCreate with the protocol object as its owner): a combo box listing the groups (filled when the box
// is shown) and OK / Cancel. The contact is the record kept in the box (set from outside before it is shown). OK asks the
// protocol object to move the contact to the chosen group and closes the box.

#include "nwindow/NMsnDlgCommon.h"
#include "nwindow/NMsnBuddy.h"
#pragma pack(push, 4)

enum
{
    NID_MOVEGROUP_OK = 0xA23,      // names ours (the combo box has no id)
    NID_MOVEGROUP_CANCEL = 0xA24,
};

class MoveGroupDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(MoveGroupDialog, NCDialogBox, 0, 0x1031E8E0)

    NMsnLink* Msn;                   // +0x1F4 the protocol object (constructor argument)
    NMsnBuddy Buddy;                 // +0x1F8 the contact to move (its second number, +0x1DC, is the group it is in)
    NCComboBox* GroupCombo;          // +0x23C the groups, 156 x 17 at (50, 48) (child style NWS_DLG_COMBO)
    NCButton* OKButton;              // +0x240 id 0xA23, 76 x 23 at (51, 75)
    NCButton* CancelButton;          // +0x244 id 0xA24, at (131, 75)
    UTexture* BackTex;               // +0x248 "L2UI_CH3.dialog.system_back"

    MoveGroupDialog(NMsnLink* InMsn = NULL);                         // 0x101487f0
    // ~MoveGroupDialog                                              // 0x101488a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101474d0
    virtual INT OnCreate();                                          // 37 0x10147540
    virtual INT OnDestroy();                                         // 38 0x10145cb0
    virtual void ShowWindow();                                       // 48 0x10148220
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10145d50

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x10145e50 the contact moved to the chosen group, the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(MoveGroupDialog, Msn, 0x1F4);
NW_ASSERT_OFFSET(MoveGroupDialog, Buddy, 0x1F8);
NW_ASSERT_OFFSET(MoveGroupDialog, GroupCombo, 0x23C);
NW_ASSERT_OFFSET(MoveGroupDialog, OKButton, 0x240);
NW_ASSERT_OFFSET(MoveGroupDialog, CancelButton, 0x244);
NW_ASSERT_OFFSET(MoveGroupDialog, BackTex, 0x248);
NW_ASSERT_SIZE(MoveGroupDialog, 0x24C);

#pragma pack(pop)
