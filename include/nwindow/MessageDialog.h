#pragma once
// MessageDialog - the messenger's plain message box (UClass "MessageDialog", 0x1E8; C3 only, made by MsnWnd::ShowMessage and
// the like): a text and an OK button that closes it.

#include "nwindow/NMsnDlgCommon.h"
#pragma pack(push, 4)

enum
{
    NID_MESSAGE_OK = 0xA2F,        // name ours
};

class MessageDialog : public NCDialogBox
{
    NW_DECLARE_CLASS(MessageDialog, NCDialogBox, 0, 0x1031EE10)

    FString Message;                 // +0x1F4 the text, drawn at (11, 16) once it has a character
    NCButton* OKButton;              // +0x200 id 0xA2F, 76 x 23 at (90, 75)
    UTexture* BackTex;               // +0x204 "L2UI_CH3.dialog.system_back"

    MessageDialog();                                                 // 0x10148b50
    // ~MessageDialog                                                // 0x10148bf0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10146530
    virtual INT OnCreate();                                          // 37 0x10147fd0
    virtual INT OnDestroy();                                         // 38 0x10146590
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101465d0

    // name ours
    INT OnOK();                                                      // 0x10084ee0 (folded with NCOptionWnd's) the box closed
};
NW_ASSERT_OFFSET(MessageDialog, Message, 0x1F4);
NW_ASSERT_OFFSET(MessageDialog, OKButton, 0x200);
NW_ASSERT_OFFSET(MessageDialog, BackTex, 0x204);
NW_ASSERT_SIZE(MessageDialog, 0x208);

#pragma pack(pop)
