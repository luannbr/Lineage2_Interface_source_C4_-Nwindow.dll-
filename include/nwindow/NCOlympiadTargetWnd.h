#pragma once
// NCOlympiadTargetWnd - the small status window of the opponent's team while the player observes an olympiad match (UClass
// "COlympiadTargetWnd", 0x12C; C4 only): the three-piece small window skin, a name (an NCNameCtrl made at (12, 6)), and a CP and an
// HP bar. The desktop keeps two of them (NCConsole +0x21C / +0x220, one per side); the server's ExOlympiadUserInfo fills them
// through OnOlympiadUserInfo.
//
// Layout and virtuals from the stock constructor 0x1005FD60, OnCreate 0x1013E000, OnDestroy 0x1013DD00 and OnPaint 0x1013DDB0.
// The db calls 0x1013E000 "NCManorInfoSettingWnd::OnCreate" (the original code copied that window's guard
// string; the manor lane lists it as its own: names.csv gives it the right name).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCNameCtrl;

class NCOlympiadTargetWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCOlympiadTargetWnd, NCWnd, 0, 0x1031A548)

    UTexture* BackTex[3];            // +0xE8 "L2UI_CH3.SmallWnd.SmallWindow_Back1" .. "3": the left end, the middle, the right end
    UTexture* CpBarTex;              // +0xF4 "L2UI_CH3.SmallWnd.CpBar"
    UTexture* HpBarTex;              // +0xF8 "L2UI_CH3.SmallWnd.HpBar"
    UTexture* CpBackTex;             // +0xFC "L2UI_CH3.SmallWnd.CpBar_back"
    UTexture* HpBackTex;             // +0x100 "L2UI_CH3.SmallWnd.HpBar_back"
    NCNameCtrl* NameCtrl;            // +0x104 at (12, 6), 12 less wide than the window and 14 high (OnCreate)
    FString Name;                    // +0x108 the player's name (empty from Initialize)
    INT MaxHP;                       // +0x114 (names ours, from OnPaint and the order of ExOlympiadUserInfo's values)
    INT HP;                          // +0x118
    INT MaxCP;                       // +0x11C
    INT CP;                          // +0x120
    INT ObjectID;                    // +0x124 the first value of the packet
    INT Unknown128;                  // +0x128 (cleared by Initialize and OnCreate; nothing reads it)

    NCOlympiadTargetWnd();                                           // 0x1005FD60
    // ~NCOlympiadTargetWnd                                          // 0x1005FDE0

    virtual INT OnCreate();                                          // 37 0x1013E000
    virtual INT OnDestroy();                                         // 38 0x1013DD00
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013DDB0

    // names from the stock guard strings
    void Initialize();                                               // 0x1013DB80 name empty, the numbers 0
    void OnOlympiadUserInfo(L2ParamStack& Params);                   // 0x1013DC40 takes ExOlympiadUserInfo's values off the stack
};
NW_ASSERT_OFFSET(NCOlympiadTargetWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCOlympiadTargetWnd, NameCtrl, 0x104);
NW_ASSERT_OFFSET(NCOlympiadTargetWnd, Name, 0x108);
NW_ASSERT_OFFSET(NCOlympiadTargetWnd, MaxHP, 0x114);
NW_ASSERT_OFFSET(NCOlympiadTargetWnd, ObjectID, 0x124);
NW_ASSERT_SIZE(NCOlympiadTargetWnd, 0x12C);

#pragma pack(pop)
