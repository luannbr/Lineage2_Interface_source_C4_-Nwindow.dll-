#pragma once
// StatusWnd - the messenger's status menu (UClass "CStatusWnd", 0x10C; C3 only, no C2 source): a framed list of eight
// lines (online, busy, be right back, away, on the phone, out to lunch, appear offline, and the "options" one), 17 pixels
// each, the one under the mouse highlighted. A click on a line sets the status code on the protocol object or opens a
// dialog of the messenger window, and the menu goes (it hides itself when it loses the focus).
//
// Layout and virtuals from the stock constructor 0x10137860, OnCreate 0x101365E0 (the list is made by 0x10134EB0),
// OnDestroy 0x101352E0, OnPaint 0x10132180, OnMouseMove 0x10130DA0 and OnLButtonUp 0x10135370.

#include "nwindow/NCWnd.h"
#include "nwindow/NCListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnWnd;
class Msn;

class StatusWnd : public NCWnd
{
    NW_DECLARE_CLASS(StatusWnd, NCWnd, 0, 0x10324140)

    MsnWnd* Owner;                   // +0xE8 the messenger window that made it (the constructor's first argument) (name ours)
    Msn* MsnObj;                     // +0xEC the protocol object (the second) (name ours)
    TArray<NListItem> Items;         // +0xF0 the lines: the text, -1, and the little picture (the NListItem's third word,
                                     //   a texture here) (name ours)
    INT Hover;                       // +0xFC the line under the mouse, -1 for none (name ours)
    UTexture* FrameTex[9];           // +0x100 "L2UI_CH3.Tooltip.Tooltip1" .. "9": corners, edges and fill, row by row
    UTexture* SelectTex;             // +0x124 "L2UI_CH3.ListCtrl.TextSelect": the highlight's left part
    UTexture* SelectTex2;            // +0x128 "L2UI_CH3.ListCtrl.TextSelect2": its right end

    StatusWnd(MsnWnd* InOwner = NULL, Msn* InMsn = NULL);                          // 0x1015c1d0 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~StatusWnd                                                    // 0x1015c270

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, shared)
    virtual void KillFocus();                                        // 30 0x10155a80
    virtual INT OnCreate();                                          // 37 0x1015afd0
    virtual INT OnDestroy();                                         // 38 0x10159ea0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10156e10
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10155a10
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10159f30 (the db says OnLButtonDown: a click acts on release)

    void BuildItems();                                               // 0x10159a70 (name ours) the eight lines
};
NW_ASSERT_OFFSET(StatusWnd, Owner, 0xE8);
NW_ASSERT_OFFSET(StatusWnd, MsnObj, 0xEC);
NW_ASSERT_OFFSET(StatusWnd, Items, 0xF0);
NW_ASSERT_OFFSET(StatusWnd, Hover, 0xFC);
NW_ASSERT_OFFSET(StatusWnd, FrameTex, 0x100);
NW_ASSERT_OFFSET(StatusWnd, SelectTex, 0x124);
NW_ASSERT_OFFSET(StatusWnd, SelectTex2, 0x128);
NW_ASSERT_SIZE(StatusWnd, 0x12C);

#pragma pack(pop)
