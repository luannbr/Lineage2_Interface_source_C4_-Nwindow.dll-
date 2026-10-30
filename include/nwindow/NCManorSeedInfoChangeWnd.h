#pragma once
// NCManorSeedInfoChangeWnd - the box that sets one seed's amount and price in the lord's table (new in C4; UClass
// "CManorSeedInfoChangeWnd", 0x220, a modal NCDialogBox; the desktop keeps it at +0x200). NCManorSeedInfoSettingWnd's change
// button fills it (SetSeedName, SetAmt, SetPrice, SetLimits) and shows it modally (DoModal); OK checks the numbers and hands
// them back (NCManorSeedInfoSettingWnd::NotifyChangeTomValue) or reports the error (system messages 0x615 / 0x616); Cancel
// closes it. It is NCManorCropInfoChangeWnd's twin without the price kind (no combo box): the same fields four bytes lower.

#include "nwindow/NCDialogBox.h"
#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the two buttons (names ours; the same as the crop box's)
enum
{
    NID_MANORSEEDCHANGE_OK = (INT)0xFFDCCC00,
    NID_MANORSEEDCHANGE_CANCEL = (INT)0xFFDCCC01,
};

class NCManorSeedInfoChangeWnd : public NCDialogBox
{
    NW_DECLARE_CLASS(NCManorSeedInfoChangeWnd, NCDialogBox, 0, 0x103180C0)

    NCButton* OKButton;              // +0x1F4 id 0xFFDCCC00, text 140, at (51, 172)
    NCButton* CancelButton;          // +0x1F8 id 0xFFDCCC01, text 141, at (131, 172)
    NCEditBox* AmountEdit;           // +0x1FC id 0xFFDCCC10, at (145, 68), 68 x 17, 10 characters
    NCEditBox* PriceEdit;            // +0x200 id 0xFFDCCC11, at (145, 118), 68 x 17, 10 characters
    UTexture* BackTex;               // +0x204 "L2UI_CH3.Manor.manor4_back"
    FString SeedName;                // +0x208 the seed being edited
    INT MaxAmount;                   // +0x214 the most the table has (cleared by OnCreate; SetMaxAmount sets it)
    INT MinPrice;                    // +0x218 the price may not go below / above these two
    INT MaxPrice;                    // +0x21C

    NCManorSeedInfoChangeWnd();                                      // 0x1005f880
    // ~NCManorSeedInfoChangeWnd                                     // 0x1005f900

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10138390
    virtual INT OnCreate();                                          // 37 0x10138730
    virtual INT OnDestroy();                                         // 38 0x10138400
    virtual void HideWindow();                                       // 49 0x10137d70
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10138530
    virtual void NewVirtual75();                                     // 75 0x10137de0 DoModal: the amount edit takes the focus, then the base's

    // names ours
    void SetSeedName(FString Name);                                  // 0x10137e60 (ret 0xc)
    // SetMaxAmount(INT) is 0x1000C810 (ret 4): the linker folded it with NCEditBox::SetType (both are mov [ecx+0x214], arg), the
    // setting window calls it by that address
    void SetAmt(FString Text);                                       // 0x10137ef0 (ret 0xc) the text of the amount edit
    void SetPrice(FString Text);                                     // 0x10137f90 (ret 0xc) the text of the price edit
    void SetLimits(INT InMinPrice, INT InMaxPrice);                  // 0x10138030 (ret 8; anonymous in the db)
    INT OnCancelBtnClick();                                          // 0x10138310
    INT OnOKBtnClick();                                              // 0x10138050
};
NW_ASSERT_OFFSET(NCManorSeedInfoChangeWnd, OKButton, 0x1F4);
NW_ASSERT_OFFSET(NCManorSeedInfoChangeWnd, SeedName, 0x208);
NW_ASSERT_OFFSET(NCManorSeedInfoChangeWnd, MaxAmount, 0x214);
NW_ASSERT_OFFSET(NCManorSeedInfoChangeWnd, MaxPrice, 0x21C);
NW_ASSERT_SIZE(NCManorSeedInfoChangeWnd, 0x220);

#pragma pack(pop)
