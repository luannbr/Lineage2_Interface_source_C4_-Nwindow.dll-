#pragma once
// NCManorSeedInfoWnd - the seed prices of a manor (new in C4; UClass "CManorSeedInfoWnd", 0x108; the desktop keeps it with the
// other manor windows). NCManorCropInfoWnd's twin: a list of the seeds (name, the amount, the price, ..., the items each
// is made from / gives), a combo box for the manor and one for the period (this month / next month) and the buy button.
// The stock code derives it from NCManorInfoDownWnd (items2: the shared OnCreate 0x10136960 makes the list and the back
// texture, OnPaint 0x10136860 draws the back texture, OnDestroy 0x101367B0 removes the list); this header keeps NCWnd as the
// base and declares the two inherited fields, the shared bodies are called by address.
//
// The console's ReceiveManorSeedInfo fills it (OnShowSeedInfo); the combo boxes ask the server for another page
// ("manor_menu_select?ask=3&state=<manor>&time=<period>"), the buy button for the buy page (ask=1).

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids of the controls (names ours)
enum
{
    NID_MANORSEED_BUY = (INT)0xFFDDCCB0,
    NID_MANORSEED_TIMECOMBO = (INT)0xFFDDCCA0,
    NID_MANORSEED_MANORCOMBO = (INT)0xFFDDCCA1,   // the window procedure answers it; both combo boxes are made with 0xFFDDCCA0
};

class NCManorSeedInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorSeedInfoWnd, NCWnd, 0, 0x10318B30)

    NCListCtrl* ListCtrl;            // +0xE8 (NCManorInfoDownWnd's) id 0xF0000001
    UTexture* BackTex;               // +0xEC (NCManorInfoDownWnd's) "L2UI_CH3.Manor.manor1_back2"
    NCComboBox* ManorCombo;          // +0xF0 the manors (id 0xFFDDCCA0), at (354, 12), 130 x 17
    NCButton* BuyButton;             // +0xF4 id 0xFFDDCCB0, text 0x45F: hidden on the manor's own page
    NCComboBox* TimeCombo;           // +0xF8 id 0xFFDDCCA0, items 0x45C / 0x45D, at (164, 12), 70 x 17
    INT ManorID;                     // +0xFC the manor shown (the packet's second value)
    INT PaintManorID;                // +0x100 -1 from OnCreate; the manor whose name OnPaint writes under the title (SetPaintManor: NCManorInfoWnd sets it to ManorID when the window comes up)
    BYTE bOwnManor;                  // +0x104 the packet's first value: 1 = the player's own manor's page (hides the buy button)

    NCManorSeedInfoWnd();                                            // 0x10137980
    // ~NCManorSeedInfoWnd                                           // 0x101379d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013b540
    virtual INT OnCreate();                                          // 37 0x1013afd0
    virtual INT OnDestroy();                                         // 38 0x1013ac20
    virtual void ShowWindow();                                       // 48 0x1013a570
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013ad20
    virtual INT NewVirtual74(INT Row);                               // 74 0x100023c0 (shared stub, ret 4: the row of the list's command, unused)

    // names ours
    void OnShowSeedInfo(L2ParamStack* Params);                       // 0x1013a6e0 (ret 4) the list from the console's packet
    INT OnBuySeedBtnClick();                                         // 0x1013a610 asks for the buy page
    INT GetSelectedManorID();                                        // 0x1013ab90
    __declspec(noinline) void SetPaintManor();                       // 0x1013a600 (anonymous in the db) PaintManorID = ManorID
    __declspec(noinline) INT GetTimeSelected();                      // 0x1013ac10 (anonymous; the crop page shares it) the period combo box's choice
    __declspec(noinline) INT RequestData();                                               // 0x1013aef0 asks for the page of the manor and period chosen
    INT OnManorComboBox(WPARAM lParam);                              // 0x1013b440 (ret 4)
    INT OnTimeComboBox(WPARAM lParam);                               // 0x1013b4c0 (ret 4)
};
NW_ASSERT_OFFSET(NCManorSeedInfoWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorSeedInfoWnd, ManorCombo, 0xF0);
NW_ASSERT_OFFSET(NCManorSeedInfoWnd, ManorID, 0xFC);
NW_ASSERT_OFFSET(NCManorSeedInfoWnd, bOwnManor, 0x104);
NW_ASSERT_SIZE(NCManorSeedInfoWnd, 0x108);

#pragma pack(pop)
