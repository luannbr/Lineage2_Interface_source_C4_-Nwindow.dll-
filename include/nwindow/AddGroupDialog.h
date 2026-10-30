#pragma once
// AddGroupDialog - the box that asks for the name of a new contact group (UClass "AddGroupDialog", 0x1E8; C3 only, made by
// MsnWnd::OnCreate with the protocol object as its owner): an edit line and OK / Cancel. OK sends the name to the
// protocol object and closes the box; showing it empties the line. The same layout and code as AddFriendDialog.

#include "nwindow/NMsnDlgCommon.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NMsnLink.h"
#pragma pack(push, 4)

enum
{
    NID_ADDGROUP_EDIT = 0xA25,     // names ours
    NID_ADDGROUP_OK = 0xA26,
    NID_ADDGROUP_CANCEL = 0xA27,
};

class AddGroupDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(AddGroupDialog, NCDialogBox, 0, 0x1031E3A8)

    NMsnLink* Msn;                   // +0x1F4 the protocol object (constructor argument)
    NCEditBox* EditBox;              // +0x1F8 the name, 156 x 17 at (50, 48)
    NCButton* OKButton;              // +0x1FC
    NCButton* CancelButton;          // +0x200
    UTexture* BackTex;               // +0x204 "L2UI_CH3.dialog.system_back"

    AddGroupDialog(NMsnLink* InMsn = NULL);                          // 0x10148900
    // ~AddGroupDialog                                               // 0x10148970

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10145eb0
    virtual INT OnCreate();                                          // 37 0x10147780
    virtual INT OnDestroy();                                         // 38 0x10145f20 (AddFriendDialog's body)
    virtual void ShowWindow();                                       // 48 0x101451d0 (AddFriendDialog's body)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10145fc0

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x101451f0 the name sent, the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(AddGroupDialog, Msn, 0x1F4);
NW_ASSERT_OFFSET(AddGroupDialog, EditBox, 0x1F8);
NW_ASSERT_OFFSET(AddGroupDialog, OKButton, 0x1FC);
NW_ASSERT_OFFSET(AddGroupDialog, CancelButton, 0x200);
NW_ASSERT_OFFSET(AddGroupDialog, BackTex, 0x204);
NW_ASSERT_SIZE(AddGroupDialog, 0x208);

#pragma pack(pop)
