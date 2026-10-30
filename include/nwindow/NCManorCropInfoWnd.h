#pragma once
// NCManorCropInfoWnd - the crop prices of a manor (new in C4; UClass "CManorCropInfoWnd", 0x104; the desktop keeps it with
// the other manor windows). A list of the crops (name, price, ..., the seeds / crops each is made from), a combo box for
// the manor and one for the period (this month / next month), the view button and the sell button. The stock code derives
// it from NCManorInfoDownWnd (items2: the shared OnCreate 0x10136960 makes the list and the back texture, OnPaint
// 0x10136860 draws the back texture, OnDestroy 0x101367B0 removes the list); this header keeps NCWnd as the base and
// declares the two inherited fields, the shared bodies are called by address.
//
// The console's ReceiveManorCropInfo fills it (OnShowCropInfo); the combo boxes and the view button ask the server for
// another page ("manor_menu_select?ask=4&state=<manor>&time=<period>").

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the controls (names ours)
enum
{
    NID_MANORCROP_SELL = (INT)0xFFDECCA0,
    NID_MANORCROP_TIMECOMBO = (INT)0xFFDECCB0,
    NID_MANORCROP_MANORCOMBO = (INT)0xFFDECCB1,   // the window procedure answers it; both combo boxes are made with 0xFFDECCB0
};

class NCManorCropInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorCropInfoWnd, NCWnd, 0, 0x10315708)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoDownWnd's) id 0xF0000001
    UTexture* BackTex;               // +0xEC (NCManorInfoDownWnd's) "L2UI_CH3.Manor.manor1_back2"
    NCComboBox* ManorCombo;          // +0xF0 the manors (id 0xFFDECCB0), at (354, 12), 130 x 17
    NCButton* SellButton;            // +0xF4 id 0xFFDECCA0, text 0x46A: hidden on the manor's own page
    NCComboBox* TimeCombo;           // +0xF8 id 0xFFDECCB0, items 0x45C / 0x45D, at (164, 12), 70 x 17
    INT ManorID;                     // +0xFC the manor shown (the packet's second value)
    BYTE bOwnManor;                  // +0x100 the packet's first value: 1 = the player's own manor's page (hides the sell button)

    NCManorCropInfoWnd();                                            // 0x101334d0
    // ~NCManorCropInfoWnd                                           // 0x10133520 (the base's destructor 0x10133430 follows)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10133390
    virtual INT OnCreate();                                          // 37 0x10132e00
    virtual INT OnDestroy();                                         // 38 0x10132b30
    virtual void ShowWindow();                                       // 48 0x101323e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10132c30
    virtual INT NewVirtual74(INT Row);                               // 74 0x100023c0 (shared stub, ret 4: the row of the list's command, unused)

    // names ours
    void OnShowCropInfo(L2ParamStack* Params);                       // 0x10132630 (ret 4) the list from the console's packet
    INT OnSellCropBtnClick();                                        // 0x10132470 asks for the sell page
    void ValidateManorComboBox();                                    // 0x10132540 the manors the console knows; the shown one selected
    INT GetSelectedManorID();                                        // 0x10132ab0
    INT OnViewBtnClick();                                            // 0x10132d20 asks for the page of the manor and period chosen
    INT OnManorComboBox(WPARAM lParam);                              // 0x10133290 (ret 4)
    INT OnTimeComboBox(WPARAM lParam);                               // 0x10133310 (ret 4)
};
NW_ASSERT_OFFSET(NCManorCropInfoWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorCropInfoWnd, ManorCombo, 0xF0);
NW_ASSERT_OFFSET(NCManorCropInfoWnd, ManorID, 0xFC);
NW_ASSERT_OFFSET(NCManorCropInfoWnd, bOwnManor, 0x100);
NW_ASSERT_SIZE(NCManorCropInfoWnd, 0x104);

#pragma pack(pop)
