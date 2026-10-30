#pragma once
// NCManorCropInfoChangeWnd - the box that sets one crop's amount and price in the lord's table (new in C4; UClass
// "CManorCropInfoChangeWnd", 0x224, a modal NCDialogBox; the desktop keeps it at +0x204). NCManorCropInfoSettingWnd's change
// button fills it (SetCropName, SetMaxAmount, SetAmt, SetPrice, SetLimits) and shows it modally (DoModal); OK checks the
// numbers and hands them back (NCManorCropInfoSettingWnd::NotifyChangeTomValue) or reports the error (system messages
// 0x617 / 0x618); Cancel closes it.

#include "nwindow/NCDialogBox.h"
#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the two buttons (names ours)
enum
{
    NID_MANORCHANGE_OK = (INT)0xFFDCCC00,
    NID_MANORCHANGE_CANCEL = (INT)0xFFDCCC01,
};

class NCManorCropInfoChangeWnd : public NCDialogBox
{
    NW_DECLARE_CLASS(NCManorCropInfoChangeWnd, NCDialogBox, 0, 0x10314C98)

    NCButton* OKButton;              // +0x1F4 id 0xFFDCCC00, text 140, at (51, 172)
    NCButton* CancelButton;          // +0x1F8 id 0xFFDCCC01, text 141, at (131, 172)
    NCEditBox* AmountEdit;           // +0x1FC id 0xFFDCCC10, at (145, 68), 68 x 17, 10 characters
    NCEditBox* PriceEdit;            // +0x200 id 0xFFDCCC11, at (145, 118), 68 x 17, 10 characters
    UTexture* BackTex;               // +0x204 "L2UI_CH3.Manor.manor4_back"
    NCComboBox* PriceCombo;          // +0x208 id 0xFFDCCC30, at (145, 143), 68 x 17: the kind of the price ("1" / "2")
    FString CropName;                // +0x20C the crop being edited
    INT MaxAmount;                   // +0x218 the most the crop table has
    INT MinPrice;                    // +0x21C the price may not go below / above these two
    INT MaxPrice;                    // +0x220

    NCManorCropInfoChangeWnd();                                      // 0x1005f970
    // ~NCManorCropInfoChangeWnd                                     // 0x1005f9f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101301f0
    virtual INT OnCreate();                                          // 37 0x10130260
    virtual INT OnDestroy();                                         // 38 0x1012fb80
    virtual void HideWindow();                                       // 49 0x1012f780
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1012fcd0
    virtual void NewVirtual75();                                     // 75 0x1012f7f0 DoModal: the amount edit takes the focus, then the base's

    // names ours
    void SetCropName(FString Name);                                  // 0x1012f870 (ret 0xc)
    void SetMaxAmount(INT Value);                                    // 0x1012f900 (ret 4; anonymous in the db)
    void SetAmt(FString Text);                                       // 0x1012f910 (ret 0xc) the text of the amount edit
    void SetPriceText(FString Text);                                 // 0x1012f9b0 (ret 0xc) the text of the price edit (db: SetPrice)
    void SetPriceKind(INT Kind);                                     // 0x1012fa50 (ret 4) the combo box's choice (db: SetPrice; 0 = the first)
    void SetLimits(INT InMinPrice, INT InMaxPrice);                  // 0x1012fae0 (ret 8; anonymous in the db)
    INT OnCancelBtnClick();                                          // 0x1012fb00
    INT OnOKBtnClick();                                              // 0x1012ff00
};
NW_ASSERT_OFFSET(NCManorCropInfoChangeWnd, OKButton, 0x1F4);
NW_ASSERT_OFFSET(NCManorCropInfoChangeWnd, CropName, 0x20C);
NW_ASSERT_OFFSET(NCManorCropInfoChangeWnd, MaxAmount, 0x218);
NW_ASSERT_SIZE(NCManorCropInfoChangeWnd, 0x224);

#pragma pack(pop)
