#pragma once
// NCBroadcastGMFenceWnd - the GM's "fence" window of the broadcast (event match) tools (UClass "CBroadcastGMFenceWnd", 0x1F8;
// new in C3: the desktop NCConsole makes it at +0x1B0 next to NCBroadcastGMWnd at +0x1AC). A small dialog (the stock
// constructor 0x1010CA70 starts from NCDialogBox's 0x1000A130, although the UClass is registered under NCWnd) where the GM types
// where the match's fence goes: three edit boxes for the position (X, Y, Z), two more for the size (each 100 .. 4999), and
// the buttons OK (hands the five texts to NCBroadcastGMWnd::NotifyFenceInfo, 0x1010E3C0, which sends "eventmatch create 1 ..."
// to the server), Cancel (closes the modal box) and a third one that fills the three position boxes with the player's place.
//
// Layout and virtuals from the stock constructor 0x1010CA70, OnCreate 0x1010C3D0, OnDestroy 0x1010C070, OnPaint 0x1010C240 and
// the handlers 0x1010BC20 .. 0x1010BFA1.

#include "nwindow/NCDialogBox.h"
#pragma pack(push, 4)

class NCButton;
class NCEditBox;

class NCBroadcastGMFenceWnd : public NCDialogBox
{
    NW_DECLARE_CLASS(NCBroadcastGMFenceWnd, NCDialogBox, 0, 0x10311348)

    UTexture* BackTex;               // +0x1F4 "L2UI_CH3.Minimap.MapBack" (OnCreate): the panel under the title bar
    NCButton* OKButton;              // +0x1F8 id 0xFFFFE001 (text: SysString 140)
    NCButton* CancelButton;          // +0x1FC id 0xFFFFE002 (text: SysString 141)
    NCButton* MyPosButton;           // +0x200 id 0xFFFFE003 (name ours): the position boxes take the player's place
    NCEditBox* XEdit;                // +0x204 (names ours for the five boxes; all five have the id 0xFFFFD001)
    NCEditBox* YEdit;                // +0x208
    NCEditBox* ZEdit;                // +0x20C
    NCEditBox* WidthEdit;            // +0x210 100 .. 4999
    NCEditBox* HeightEdit;           // +0x214 100 .. 4999

    NCBroadcastGMFenceWnd();                                         // 0x101229b0
    // ~NCBroadcastGMFenceWnd                                        // 0x10122a50

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10121ed0
    virtual INT OnCreate();                                          // 37 0x101222d0
    virtual INT OnDestroy();                                         // 38 0x10121f60 (NCDialogBox's is not called)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10122140

    // handlers of the three buttons (the stock guard strings; the third one carries the Cancel string too)
    INT OnOKBtnClick();                                              // 0x10121b10
    INT OnCancelBtnClick();                                          // 0x10121cc0
    INT OnMyPosBtnClick();                                           // 0x10121d40 (name ours; guard string OnCancelBtnClick)
};
NW_ASSERT_OFFSET(NCBroadcastGMFenceWnd, BackTex, 0x1F4);
NW_ASSERT_OFFSET(NCBroadcastGMFenceWnd, OKButton, 0x1F8);
NW_ASSERT_OFFSET(NCBroadcastGMFenceWnd, XEdit, 0x204);
NW_ASSERT_OFFSET(NCBroadcastGMFenceWnd, HeightEdit, 0x214);
NW_ASSERT_SIZE(NCBroadcastGMFenceWnd, 0x218);

#pragma pack(pop)
