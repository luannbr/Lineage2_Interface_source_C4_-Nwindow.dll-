#pragma once
// NCManorSeedInfoSettingWnd - the seed prices of the lord's manor, to be set (new in C4; UClass "CManorSeedInfoSettingWnd",
// 0x11C; the desktop keeps it at +0x1F0). A list of the seeds (name, ..., the amount, the price, ...), five buttons (change,
// set to today, clear, OK, cancel) and, under the list, the total of the default prices. NCManorCropInfoSettingWnd's twin: the
// stock code derives both from NCManorInfoSettingWnd (the shared OnCreate 0x10136CE0, OnPaint 0x10136BE0, OnDestroy
// 0x10136B30 and WindowProc 0x10136AD0); this header keeps NCWnd as the base and declares the two inherited fields, the
// shared bodies are called by address.
//
// The console's ReceiveManorSeedSetting fills it (OnShowSeedSetting); the change button opens NCManorSeedInfoChangeWnd for
// the selected row (the desktop keeps that window at +0x200), which hands the new values back (NotifyChangeTomValue);
// OK sends the whole table (RequestSetSeed).

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the buttons and the dialog answers (names ours)
enum
{
    NID_MANORSEEDSET_CHANGE = (INT)0xFFDDCDA0,
    NID_MANORSEEDSET_TODAY = (INT)0xFFDDCDA1,
    NID_MANORSEEDSET_CLEAR = (INT)0xFFDDCDA2,
    NID_MANORSEEDSET_OK = (INT)0xFFDDCDA3,
    NID_MANORSEEDSET_CANCEL = (INT)0xFFDDCDA4,
    NWM_MANORSEEDSET_CLEARCONFIRM = (INT)0xF000001C,   // the answers of the dialog boxes 0x56 / 0x57 (the crop window's too)
    NWM_MANORSEEDSET_TODAYCONFIRM = (INT)0xF000001D,
};

class NCManorSeedInfoSettingWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorSeedInfoSettingWnd, NCWnd, 0, 0x103185F8)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoSettingWnd's)
    UTexture* BackTex;               // +0xEC (NCManorInfoSettingWnd's)
    NCButton* ChangeButton;          // +0xF0 id 0xFFDDCDA0, text 0x476, at (8, 297)
    NCButton* TodayButton;           // +0xF4 id 0xFFDDCDA1, text 0x477, at (88, 297)
    NCButton* ClearButton;           // +0xF8 id 0xFFDDCDA2, text 0x478, at (168, 297)
    NCButton* OKButton;              // +0xFC id 0xFFDDCDA3, text 140, at (438, 297)
    NCButton* CancelButton;          // +0x100 id 0xFFDDCDA4, text 141, at (518, 297)
    UTexture* AdenaTex;              // +0x104 "L2UI.WindowIcon.WindowIconAdena"
    UTexture* TextBoxTex[3];         // +0x108 "L2UI_CH3.Etc.textbox1".."3": the box under the list
    INT ManorID;                     // +0x114 the manor (-1 until the console fills the window)
    INT TotalPrice;                  // +0x118 the sum of amount * default price over the rows (CalcDefaultPriceSum)

    NCManorSeedInfoSettingWnd();                                     // 0x1005f570
    // ~NCManorSeedInfoSettingWnd                                    // 0x1005f5c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013a460
    virtual INT OnCreate();                                          // 37 0x10139300
    virtual INT OnDestroy();                                         // 38 0x10138c00
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10138d50
    // name ours: the list reports a double click on a row
    virtual INT OnListCtrl(INT Row);                                 // 74 0x10139970 (ret 4) the same as the change button

    // names ours
    INT OnCancelBtnClick();                                          // 0x10138b80 closes the window
    __declspec(noinline) INT OnChangeBtnClick();                                          // 0x10138f60 hands the selected row to the change window
    INT OnSetToTodayBtnClick();                                      // 0x10139140 asks to set the prices to today's
    INT OnClearBtnClick();                                           // 0x10139220 asks to clear the table
    INT OnOKBtnClick();                                              // 0x101399f0 sends the table
    void CalcDefaultPriceSum();                                      // 0x10139b90
    void NotifyChangeTomValue(INT Amount, INT Price);                // 0x10139c80 (ret 8) the change window's answer for the row
    void OnShowSeedSetting(L2ParamStack* Params);                    // 0x10139d40 (ret 4) the table from the console's packet
    INT OnClearConfirmMsg(WPARAM wParam, LPARAM lParam);             // 0x1013a250 (ret 8) the amounts and prices of the rows emptied
    INT OnTodayConfirmMsg(WPARAM wParam, LPARAM lParam);             // 0x1013a340 (ret 8) the prices copied from the default column
};
NW_ASSERT_OFFSET(NCManorSeedInfoSettingWnd, ChangeButton, 0xF0);
NW_ASSERT_OFFSET(NCManorSeedInfoSettingWnd, AdenaTex, 0x104);
NW_ASSERT_OFFSET(NCManorSeedInfoSettingWnd, ManorID, 0x114);
NW_ASSERT_OFFSET(NCManorSeedInfoSettingWnd, TotalPrice, 0x118);
NW_ASSERT_SIZE(NCManorSeedInfoSettingWnd, 0x11C);

#pragma pack(pop)
