#pragma once
// NCManorDefaultInfoWnd - the manor information page of the crops a manor can be asked to grow (new in C4; UClass
// "CManorDefaultInfoWnd", 0xFC; the desktop keeps it with the other manor windows, NCManorInfoWnd makes it as its third page).
// A list (the item, ..., the seeds / crops it is made from / gives), the buy seed button and the sell crop button; both
// buttons are hidden on the player's own manor's page. The stock code derives it from NCManorInfoDownWnd (items2: the shared
// OnCreate 0x10136960 makes the list and the back texture, OnPaint 0x10136860 draws the back texture, OnDestroy 0x101367B0
// removes the list); this header keeps NCWnd as the base and declares the two inherited fields, the shared bodies are called
// by address.
//
// The console's ReceiveManorDefaultInfo fills it through NCManorInfoWnd (OnShowManorDefaultInfo).

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the two buttons (names ours)
enum
{
    NID_MANORDEFAULT_BUYSEED = (INT)0xFADDCCA0,
    NID_MANORDEFAULT_SELLCROP = (INT)0xFADDCCA1,
};

class NCManorDefaultInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorDefaultInfoWnd, NCWnd, 0, 0x103166A8)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoDownWnd's) id 0xF0000001
    UTexture* BackTex;               // +0xEC (NCManorInfoDownWnd's) "L2UI_CH3.Manor.manor1_back2"
    NCButton* BuySeedButton;         // +0xF0 id 0xFADDCCA0, text 0x45F, at (438, 249)
    NCButton* SellCropButton;        // +0xF4 id 0xFADDCCA1, text 0x46A, at (518, 249)
    BYTE bOwnManor;                  // +0xF8 the packet's first value: 1 = the player's own manor's page (hides both buttons)

    NCManorDefaultInfoWnd();                                         // 0x10136700
    // ~NCManorDefaultInfoWnd                                        // 0x10136750

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10135cf0
    virtual INT OnCreate();                                          // 37 0x10136300
    virtual INT OnDestroy();                                         // 38 0x10136140
    virtual void ShowWindow();                                       // 48 0x10135ab0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10136210
    virtual INT NewVirtual74(INT Row);                               // 74 0x100023c0 (shared stub, ret 4: the row of the list's command, unused)

    // names ours
    INT OnBuySeedBtnClick();                                         // 0x10135b50 asks for the buy page
    INT OnSellCropBtnClick();                                        // 0x10135c20 asks for the sell page
    void OnShowManorDefaultInfo(L2ParamStack* Params);               // 0x10135d60 (ret 4) the list from the console's packet
};
NW_ASSERT_OFFSET(NCManorDefaultInfoWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorDefaultInfoWnd, BuySeedButton, 0xF0);
NW_ASSERT_OFFSET(NCManorDefaultInfoWnd, bOwnManor, 0xF8);
NW_ASSERT_SIZE(NCManorDefaultInfoWnd, 0xFC);

#pragma pack(pop)
