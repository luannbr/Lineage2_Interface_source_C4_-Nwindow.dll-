#pragma once
// OfflineConfirmDialog - the question before the messenger goes offline (UClass "OfflineConfirmDialog", 0x1E4; C3 only,
// made by MsnWnd::OnCreate with the window as its owner): a text and OK / Cancel. OK takes the messenger offline
// (MsnWnd 0x101382A0) and closes the box; Cancel closes it.

#include "nwindow/NMsnDlgCommon.h"
#pragma pack(push, 4)

enum
{
    NID_OFFLINE_OK = 0xA2D,        // names ours
    NID_OFFLINE_CANCEL = 0xA2E,
};

class OfflineConfirmDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(OfflineConfirmDialog, NCDialogBox, 0, 0x1031F340)

    MsnWnd* Owner;                   // +0x1F4 the messenger window (constructor argument)
    NCButton* OKButton;              // +0x1F8 id 0xA2D, 76 x 23 at (51, 75)
    NCButton* CancelButton;          // +0x1FC id 0xA2E, at (131, 75)
    UTexture* BackTex;               // +0x200 "L2UI_CH3.dialog.system_back"

    OfflineConfirmDialog(MsnWnd* InOwner = NULL);                    // 0x10148470
    // ~OfflineConfirmDialog                                         // 0x101484e0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10145250
    virtual INT OnCreate();                                          // 37 0x10146740
    virtual INT OnDestroy();                                         // 38 0x101452c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10145330

    // names ours
    __declspec(noinline) INT OnOK();                                                      // 0x10145150 the messenger offline, the box closed
    __declspec(noinline) INT OnCancel();                                                  // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(OfflineConfirmDialog, Owner, 0x1F4);
NW_ASSERT_OFFSET(OfflineConfirmDialog, OKButton, 0x1F8);
NW_ASSERT_OFFSET(OfflineConfirmDialog, CancelButton, 0x1FC);
NW_ASSERT_OFFSET(OfflineConfirmDialog, BackTex, 0x200);
NW_ASSERT_SIZE(OfflineConfirmDialog, 0x204);

#pragma pack(pop)
