#pragma once
// VersionWnd - the messenger's "version" box (UClass "CVersionWnd", 0xD4; C3 only, no C2 source): a picture with three
// lines of text (system messages 1310 .. 1312) and one button that closes it and shows the log-in window again. The
// messenger window makes it (0x10139A40) and shows it from the log-in window's "version" button.
//
// Layout and virtuals from the stock constructor 0x101376F0, OnCreate 0x10134DC0, WindowProc 0x10134D60 and OnPaint
// 0x10131A90.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnWnd;
class NCButton;

class VersionWnd : public NCWnd
{
    NW_DECLARE_CLASS(VersionWnd, NCWnd, 0, 0x10324670)

    MsnWnd* Owner;                   // +0xE8 the messenger window that made it (the constructor's argument) (name ours)
    NCButton* OKButton;              // +0xEC id 0xA54, 96 x 23 at (79, 274), the label is system string 1008
    UTexture* BackTex;               // +0xF0 "L2UI_CH3.Msn.messenger_back1"

    VersionWnd(MsnWnd* InOwner = NULL);                                     // 0x1015c060 (ret 4); defaults = what the stock InternalConstructor pushes
    // ~VersionWnd                                                   // 0x1015c0c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10159920
    virtual INT OnCreate();                                          // 37 0x10159980
    virtual INT OnDestroy();                                         // 38 0x101566e0 (NCSSQMainEventWnd's body, shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10156720

    __declspec(noinline) INT OnOKButton();                                                // 0x10156870 (name ours) hides it, the owner shows the log-in window
};
NW_ASSERT_OFFSET(VersionWnd, Owner, 0xE8);
NW_ASSERT_OFFSET(VersionWnd, OKButton, 0xEC);
NW_ASSERT_OFFSET(VersionWnd, BackTex, 0xF0);
NW_ASSERT_SIZE(VersionWnd, 0xF4);

#pragma pack(pop)
