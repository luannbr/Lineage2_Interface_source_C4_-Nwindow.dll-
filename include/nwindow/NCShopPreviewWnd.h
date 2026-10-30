#pragma once
// NCShopPreviewWnd - the "try on" shop (UClass "CShopPreviewWnd", 0x108; new in C3; desktop +0x114, NCConsole::
// ShopPreviewWnd): the NPC's shop window (NCShopWnd.h) in its own mode, SHOPMODE_Preview: the same two grids
// (NCShopItemWnd) and arrows, but what moves from the NPC's list to the lower one is not bought, it is tried on - the
// goods leave the NPC's list and come back when they are put back. OK asks first (a message box, dialog 0x47); the
// answer sends the lower grid's item ids to the server (RequestPreviewItem: the NPC's object id, the list id, the
// count, the ids) and the window closes.
//
// What differs from the shop's code: the labels (sysstrings 811, 812, 813), the window procedure (the box's answer is
// the virtual at slot 73), OnOK (the question), and the NPC's object id kept at +0x104.

#include "nwindow/NCShopWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCShopPreviewWnd : public NCShopWnd
{
    NW_DECLARE_CLASS(NCShopPreviewWnd, NCShopWnd, 0, 0x10306278)

    INT OwnerID;                     // +0x124 the NPC's object id (SetOwnerID: one body with the private stores' setters
                                     //   of their +0x104); sent first with the preview request

    NCShopPreviewWnd();                                              // 0x101027b0
    // ~NCShopPreviewWnd                                             // 0x10102800

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101020f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10102200
    virtual INT OnOK();                                              // 74 0x101025a0
    // new in this class (name ours): the answer of the question OnOK asks (NWM_DIALOG_RESULT)
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 79 0x10102650

    // name ours
    void SetOwnerID(INT InOwnerID);                                  // 0x101021f0 (shared body)
};
NW_ASSERT_OFFSET(NCShopPreviewWnd, OwnerID, 0x124);
NW_ASSERT_SIZE(NCShopPreviewWnd, 0x128);

#pragma pack(pop)
