#pragma once
// NCManorCropSellChangeWnd - the box that sets how many of a crop the player sells to which manor (new in C4; UClass
// "CManorCropSellChangeWnd", 0x234, a modal NCDialogBox; the desktop keeps the crop sell window it belongs to at +0x1F8
// (NCManorCropSellWnd, items2)). The console's ReceiveProcureCropDetail fills it (OnShowProcureCropDetail): one row per
// manor that buys the crop (name, price, ...), the manors also in the combo box; Max puts the most the player has, OK hands
// the amount and the row's numbers back (NCManorCropSellWnd::NotifyChangeSellValue), Cancel closes it.

#include "nwindow/NCDialogBox.h"
#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the buttons (names ours)
enum
{
    NID_MANORSELL_MAX = (INT)0xF0000A10,
    NID_MANORSELL_OK = (INT)0xF0000A11,
    NID_MANORSELL_CANCEL = (INT)0xF0000A12,
};

class NCManorCropSellChangeWnd : public NCDialogBox
{
    NW_DECLARE_CLASS(NCManorCropSellChangeWnd, NCDialogBox, 0, 0x10315C38)

    NCButton* MaxButton;             // +0x1F4 id 0xF0000A10, text 0x486, a small button at (196, 240)
    NCButton* OKButton;              // +0x1F8 id 0xF0000A11, text 140, at (91, 277)
    NCButton* CancelButton;          // +0x1FC id 0xF0000A12, text 141, at (171, 277)
    NCComboBox* ManorCombo;          // +0x200 id 0xF0000A30, at (123, 216), 138 x 17: the manors that buy (the id is the item's data)
    NCEditBox* AmountEdit;           // +0x204 id 0xF0000A20, at (123, 241), 68 x 17, 10 characters
    NCListCtrl* ListCtrl;            // +0x208 id 0xF0000A00, at (7, 92), 322 x 109: the manors and what they pay
    UTexture* BackTex;               // +0x20C "L2UI_CH3.Manor.manor3_back"
    FString Text1;                   // +0x210 the values drawn after the labels (the owner window sets them)
    FString Text2;                   // +0x21C
    FString Text3;                   // +0x228

    NCManorCropSellChangeWnd();                                      // 0x1005f750
    // ~NCManorCropSellChangeWnd                                     // 0x1005f7f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10134760
    virtual INT OnCreate();                                          // 37 0x10133d60
    virtual INT OnDestroy();                                         // 38 0x10133940
    virtual void HideWindow();                                       // 49 0x10133580
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10133af0
    virtual void NewVirtual75();                                     // 75 0x101335f0 DoModal: the edit is emptied, the first manor chosen, the focus set

    // names ours
    INT OnCancelBtnClick();                                          // 0x101336a0
    void OnShowProcureCropDetail(L2ParamStack* Params);              // 0x10133720 (ret 4) the rows and the combo box from the console's packet
    INT OnMaxBtnClick();                                             // 0x10134490 the amount the player has of the crop, at most the row's
    INT OnOKBtnClick();                                              // 0x101345e0
};
NW_ASSERT_OFFSET(NCManorCropSellChangeWnd, MaxButton, 0x1F4);
NW_ASSERT_OFFSET(NCManorCropSellChangeWnd, BackTex, 0x20C);
NW_ASSERT_OFFSET(NCManorCropSellChangeWnd, Text1, 0x210);
NW_ASSERT_OFFSET(NCManorCropSellChangeWnd, Text3, 0x228);
NW_ASSERT_SIZE(NCManorCropSellChangeWnd, 0x234);

#pragma pack(pop)
