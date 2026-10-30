#pragma once
// NCManorCropInfoSettingWnd - the crop prices of the lord's manor, to be set (new in C4; UClass "CManorCropInfoSettingWnd",
// 0x11C). A list of the crops (name, ..., the amount, the price, ...), five buttons (change, set to today, clear,
// OK, cancel) and, under the list, the total of the default prices. The stock code derives it from NCManorInfoSettingWnd
// (items2: the shared OnCreate 0x10136CE0, OnPaint 0x10136BE0, OnDestroy 0x10136B30 and WindowProc 0x10136AD0); this header
// keeps NCWnd as the base and declares the two inherited fields, the shared bodies are called by address.
//
// The console's ReceiveManorCropSetting fills it (OnShowCropSetting); the change button opens NCManorCropInfoChangeWnd for the
// selected row (the desktop keeps that window at +0x204), which hands the new values back (NotifyChangeTomValue);
// OK sends the whole table (RequestSetCrop).

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the buttons and the dialog answers (names ours)
enum
{
    NID_MANORSET_CHANGE = (INT)0xFFDDCEA0,
    NID_MANORSET_TODAY = (INT)0xFFDDCEA1,
    NID_MANORSET_CLEAR = (INT)0xFFDDCEA2,
    NID_MANORSET_OK = (INT)0xFFDDCEA3,
    NID_MANORSET_CANCEL = (INT)0xFFDDCEA4,
    NWM_MANORSET_CLEARCONFIRM = (INT)0xF000001C,   // the answers of the dialog boxes 0x56 / 0x57
    NWM_MANORSET_TODAYCONFIRM = (INT)0xF000001D,
};

class NCManorCropInfoSettingWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorCropInfoSettingWnd, NCWnd, 0, 0x103151D0)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoSettingWnd's)
    UTexture* BackTex;               // +0xEC (NCManorInfoSettingWnd's)
    NCButton* ChangeButton;          // +0xF0 id 0xFFDDCEA0, text 0x476, at (8, 297)
    NCButton* TodayButton;           // +0xF4 id 0xFFDDCEA1, text 0x477, at (88, 297)
    NCButton* ClearButton;           // +0xF8 id 0xFFDDCEA2, text 0x47F, at (168, 297)
    NCButton* OKButton;              // +0xFC id 0xFFDDCEA3, text 140, at (438, 297)
    NCButton* CancelButton;          // +0x100 id 0xFFDDCEA4, text 141, at (518, 297)
    UTexture* AdenaTex;              // +0x104 "L2UI.WindowIcon.WindowIconAdena"
    UTexture* TextBoxTex[3];         // +0x108 "L2UI_CH3.Etc.textbox1".."3": the box under the list
    INT ManorID;                     // +0x114 the manor (-1 until the console fills the window)
    INT TotalPrice;                  // +0x118 the sum of amount * default price over the rows (CalcDefaultPriceSum)

    NCManorCropInfoSettingWnd();                                     // 0x1005f610
    // ~NCManorCropInfoSettingWnd                                    // 0x1005f660

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101322d0
    virtual INT OnCreate();                                          // 37 0x10130fc0
    virtual INT OnDestroy();                                         // 38 0x10130800
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10130950
    // name ours: the list reports a double click on a row
    virtual INT OnListCtrl(INT Row);                                 // 74 0x10131690 (ret 4) the same as the change button

    // names ours
    INT OnCancelBtnClick();                                          // 0x10130780 closes the window
    INT OnChangeBtnClick();                                          // 0x10130b60 hands the selected row to the change window
    INT OnSetToTodayBtnClick();                                      // 0x10130d70 asks to set the prices to today's
    INT OnClearBtnClick();                                           // 0x10130e50 asks to clear the table
    INT OnOKBtnClick();                                              // 0x10131710 sends the table (db: the second OnClearBtnClick)
    void CalcDefaultPriceSum();                                      // 0x101318e0
    void NotifyChangeTomValue(INT Amount, INT Price, INT Limit);     // 0x101319f0 (ret 0xc) the change window's answer for the row
    void OnShowCropSetting(L2ParamStack* Params);                    // 0x10131ad0 (ret 4) the table from the console's packet
    INT OnClearConfirmMsg(WPARAM wParam, LPARAM lParam);             // 0x10132080 (ret 8) the amounts and prices of the rows emptied
    INT OnTodayConfirmMsg(WPARAM wParam, LPARAM lParam);             // 0x10132180 (ret 8) the prices copied from the default column
};
NW_ASSERT_OFFSET(NCManorCropInfoSettingWnd, ChangeButton, 0xF0);
NW_ASSERT_OFFSET(NCManorCropInfoSettingWnd, AdenaTex, 0x104);
NW_ASSERT_OFFSET(NCManorCropInfoSettingWnd, ManorID, 0x114);
NW_ASSERT_OFFSET(NCManorCropInfoSettingWnd, TotalPrice, 0x118);
NW_ASSERT_SIZE(NCManorCropInfoSettingWnd, 0x11C);

#pragma pack(pop)
