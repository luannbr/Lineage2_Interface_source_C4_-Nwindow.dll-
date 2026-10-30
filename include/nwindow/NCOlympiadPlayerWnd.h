#pragma once
// NCOlympiadPlayerWnd - the status window of one olympiad competitor while the player observes a match (UClass "COlympiadPlayerWnd",
// 0x170; C4 only): the small window skin (the short one, or the tall one when the frame bar is expanded), a name (an NCNameCtrl),
// a CP and an HP bar, and, expanded, the last five combat messages. The desktop keeps two of them (NCConsole +0x214 / +0x218);
// ExOlympiadUserInfo fills them (OnOlympiadUserInfo) and the console's attack and skill events add messages (OnAttack,
// NotifyMagicSkillUse).
//
// Layout and virtuals from the stock constructor 0x1005FC40, OnCreate 0x1013D920, OnDestroy 0x1013D100, OnPaint 0x1013D590 and the
// methods. The db calls 0x1013D920 "NCManorInfoSettingWnd::OnCreate" (the original code copied that window's guard string; the
// manor lane lists it as its own: names.csv gives it the right name).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCOlympiadPlayerWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCOlympiadPlayerWnd, NCWnd, 0, 0x1031A010)

    UTexture* BackTex[3];            // +0xE8 "L2UI_CH3.SmallWnd.SmallWindow_Back1" .. "3": the short skin (left end, middle, right end)
    UTexture* TallTex[3];            // +0xF4 "L2UI_CH3.SmallWnd.SmallWindow3_Back1" .. "3": the same pieces, as high as the window
    UTexture* CpBarTex;              // +0x100 "L2UI_CH3.SmallWnd.CpBar"
    UTexture* HpBarTex;              // +0x104 "L2UI_CH3.SmallWnd.HpBar"
    UTexture* CpBackTex;             // +0x108 "L2UI_CH3.SmallWnd.CpBar_back"
    UTexture* HpBackTex;             // +0x10C "L2UI_CH3.SmallWnd.HpBar_back"
    FString Name;                    // +0x110 the competitor's name (empty from Initialize)
    INT MaxHP;                       // +0x11C (names ours, from OnPaint and the order of ExOlympiadUserInfo's values)
    INT HP;                          // +0x120
    INT MaxCP;                       // +0x124
    INT CP;                          // +0x128
    INT ObjectID;                    // +0x12C the first value of the packet (GetPlayerID)
    INT MsgNext;                     // +0x130 where the next message goes in Msgs (0 .. 4)
    FString Msgs[5];                 // +0x134 the last messages, a ring

    NCOlympiadPlayerWnd();                                           // 0x1005FC40
    // ~NCOlympiadPlayerWnd                                          // 0x1005FCE0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013D270
    virtual INT OnCreate();                                          // 37 0x1013D920
    virtual INT OnDestroy();                                         // 38 0x1013D100
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013D590

    // names from the stock guard strings
    void Initialize();                                               // 0x1013CEB0 nobody: the numbers 0, the name and the messages empty
    void OnOlympiadUserInfo(L2ParamStack& Params);                   // 0x1013CF70 takes ExOlympiadUserInfo's values off the stack
    void AddMsg(FString Msg);                                        // 0x1013D030 the message goes in the ring (the callee frees the string)
    INT OnFrame(INT State);                                          // 0x1013D180 the frame's expand button (0xFFFF expanded, 0xFFFE not)
    void NotifyMagicSkillUse(INT SkillID);                           // 0x1013D2D0 the "uses a skill" message (system message 46)
    void OnAttack(User* Attacker, User* Victim, INT P3, INT P4, INT P5);   // 0x1013D400 (ret 0x14)
    __declspec(noinline) INT GetPlayerID();                          // 0x1013D0F0 (an anonymous function in the db): ObjectID
};
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, CpBarTex, 0x100);
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, Name, 0x110);
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, MaxHP, 0x11C);
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, ObjectID, 0x12C);
NW_ASSERT_OFFSET(NCOlympiadPlayerWnd, Msgs, 0x134);
NW_ASSERT_SIZE(NCOlympiadPlayerWnd, 0x170);

#pragma pack(pop)
