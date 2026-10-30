#pragma once
// NCManorCropSellWnd - the crops the player can sell to a manor (new in C4; UClass "CManorCropSellWnd", 0xFC; the desktop
// keeps it at +0x1F8). A list of the crops the player has (name, ..., the amount to sell), three buttons (change, sell,
// cancel). The stock code derives it from NCManorInfoSettingWnd (items2: the shared OnCreate 0x10136CE0, OnPaint 0x10136BE0,
// OnDestroy 0x10136B30 and WindowProc 0x10136AD0); this header keeps NCWnd as the base and declares the two inherited
// fields, the shared bodies are called by address.
//
// The console's ReceiveProcureCropList fills it (OnShowSellCropList). Change asks the server for the manors that buy the
// selected crop (ask=9) and hands the names to NCManorCropSellChangeWnd (the desktop's +0x1FC), which answers with
// NotifyChangeSellValue; Sell sends the whole table (RequestProcureCropList) and closes the window.

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the buttons (names ours)
enum
{
    NID_MANORSELLCROP_CHANGE = (INT)0xFFDDCFA0,
    NID_MANORSELLCROP_SELL = (INT)0xFFDDCFA1,
    NID_MANORSELLCROP_CANCEL = (INT)0xFFDDCFA2,
};

class NCManorCropSellWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorCropSellWnd, NCWnd, 0, 0x10316170)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoSettingWnd's)
    UTexture* BackTex;               // +0xEC (NCManorInfoSettingWnd's)
    NCButton* ChangeButton;          // +0xF0 id 0xFFDDCFA0, text 0x476, at (8, 297)
    NCButton* SellButton;            // +0xF4 id 0xFFDDCFA1, text 0x485, at (438, 297)
    NCButton* CancelButton;          // +0xF8 id 0xFFDDCFA2, text 141, at (518, 297)

    NCManorCropSellWnd();                                            // 0x1005f6b0
    // ~NCManorCropSellWnd                                           // 0x1005f700

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10135a10
    virtual INT OnCreate();                                          // 37 0x10135230
    virtual INT OnDestroy();                                         // 38 0x10134f20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10135020
    // name ours: the list reports a double click on a row
    virtual INT OnListCtrl(INT Row);                                 // 74 0x10135710 (ret 4) the same as the change button

    // names ours
    INT OnCancelBtnClick();                                          // 0x10134960 closes the window
    __declspec(noinline) INT OnChangeBtnClick();                     // 0x101350e0 asks for the manors that buy the selected crop
    INT OnSellBtnClick();                                            // 0x10135790 sends the table
    INT GetMyCropCnt();                                              // 0x10134e80 how many of the selected crop the player has
    void OnShowSellCropList(L2ParamStack* Params);                   // 0x101349e0 (ret 4) the table from the console's packet
    void NotifyChangeSellValue(INT Amount, INT Manor, INT Value1, INT Value2, INT Value3);   // 0x10134800 (ret 0x14)
};
NW_ASSERT_OFFSET(NCManorCropSellWnd, ChangeButton, 0xF0);
NW_ASSERT_OFFSET(NCManorCropSellWnd, CancelButton, 0xF8);
NW_ASSERT_SIZE(NCManorCropSellWnd, 0xFC);

#pragma pack(pop)
