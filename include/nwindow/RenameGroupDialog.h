#pragma once
// RenameGroupDialog - the box that asks which contact group to rename and its new name (UClass "RenameGroupDialog",
// 0x1EC; C3 only, made by MsnWnd::OnCreate with the protocol object as its owner): a combo box listing the groups (filled
// when the box is shown), an edit line for the new name and OK / Cancel. OK sends the new name of the chosen group to the
// protocol object and closes the box.

#include "nwindow/NMsnDlgCommon.h"
#include "nwindow/NCEditBox.h"
#pragma pack(push, 4)

enum
{
    NID_RENAMEGROUP_EDIT = 0xA28,  // names ours (the combo box has no id)
    NID_RENAMEGROUP_OK = 0xA29,
    NID_RENAMEGROUP_CANCEL = 0xA2A,
};

class RenameGroupDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(RenameGroupDialog, NCDialogBox, 0, 0x1031FDA0)

    NMsnLink* Msn;                   // +0x1F4 the protocol object (constructor argument)
    NCComboBox* GroupCombo;          // +0x1F8 the groups, 78 x 17 at (50, 48) (child style NWS_DLG_COMBO)
    NCEditBox* EditBox;              // +0x1FC the new name, 78 x 17 at (135, 48)
    NCButton* OKButton;              // +0x200 id 0xA29, 76 x 23 at (51, 75)
    NCButton* CancelButton;          // +0x204 id 0xA2A, at (131, 75)
    UTexture* BackTex;               // +0x208 "L2UI_CH3.dialog.system_back"

    RenameGroupDialog(NMsnLink* InMsn = NULL);                       // 0x101489c0
    // ~RenameGroupDialog                                            // 0x10148a30

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101479d0
    virtual INT OnCreate();                                          // 37 0x10147a40
    virtual INT OnDestroy();                                         // 38 0x101460c0
    virtual void ShowWindow();                                       // 48 0x101482e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10146190

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x10146290 the chosen group renamed, the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(RenameGroupDialog, Msn, 0x1F4);
NW_ASSERT_OFFSET(RenameGroupDialog, GroupCombo, 0x1F8);
NW_ASSERT_OFFSET(RenameGroupDialog, EditBox, 0x1FC);
NW_ASSERT_OFFSET(RenameGroupDialog, OKButton, 0x200);
NW_ASSERT_OFFSET(RenameGroupDialog, CancelButton, 0x204);
NW_ASSERT_OFFSET(RenameGroupDialog, BackTex, 0x208);
NW_ASSERT_SIZE(RenameGroupDialog, 0x20C);

#pragma pack(pop)
