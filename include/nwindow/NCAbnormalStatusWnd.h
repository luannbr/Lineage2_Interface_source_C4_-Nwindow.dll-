#pragma once
// NCAbnormalStatusWnd - the buffs and debuffs on the player (UClass "CAbnormalStatusWnd",
// 0xD0): up to 31 skill icons, ten per row, behind a vertical title bar on the left. The
// console adds one per server notice (AddStatus) and clears them all (ClearStatus); a one
// second timer counts their time down, and an icon blinks during its last ten seconds. The
// tool tip of the icon under the mouse shows its name, level and description (the skin's full box).
//
// Desktop (NCConsole) +0xC8 is this window.

#include "nwindow/NCWnd.h"
#include "nwindow/NCItemWnd.h"     // NItemEntry: the window's records are built on it
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// What the console hands to AddStatus (name ours; 0x2310 bytes in C2, 0x1214 in C1: the strings are longer): the
// console builds it on its stack (AddAbnormalStatus 0x10059A00: Name from GetSkillName, IconName from
// GetSkillIconName, Desc from GetSkillDesc; Icon, ToolTip left empty) and the window copies what it needs into its
// own record (NAbnormalEntry).
struct FAbnormalStatus
{
    TCHAR Name[256];                 // +0x0000 the skill's name (C1: 128)
    TCHAR IconName[128];             // +0x0200 texture name
    TCHAR Desc[2048];                // +0x0300 the skill's description (escaped newlines) (C1: 1024)
    TCHAR ToolTip[2048];             // +0x1300 never filled (C1: "name\ndescription")
    UTexture* Icon;                  // +0x2300 NULL
    INT RemainTime;                  // +0x2304 seconds
    INT SkillID;                     // +0x2308
    INT Level;                       // +0x230C

    // inlined in the console's AddAbnormalStatus (Desc is left)
    FAbnormalStatus()
    {
        Name[0] = 0;
        IconName[0] = 0;
        ToolTip[0] = 0;
        Icon = NULL;
    }
};
NW_ASSERT_OFFSET(FAbnormalStatus, IconName, 0x200);
NW_ASSERT_OFFSET(FAbnormalStatus, Desc, 0x300);
NW_ASSERT_OFFSET(FAbnormalStatus, ToolTip, 0x1300);
NW_ASSERT_OFFSET(FAbnormalStatus, Icon, 0x2300);
NW_ASSERT_OFFSET(FAbnormalStatus, SkillID, 0x2308);
NW_ASSERT_SIZE(FAbnormalStatus, 0x2310);

// One buff on the window (name ours; allocated one by one: the window keeps pointers). An item entry for the skill
// (the fields at their C4 offsets are in NItemEntryC4.h): Icon (+0x0), Name (+0x14), the enchanted route's name in
// AdditionalName (+0x214), Description (+0x914), the skill id (+0x1B20) and its level (+0x1B24), the number its tool tip
// shows (+0x1B40: the seconds left) and the tool tip (+0x1BC0, filled with NTT_SkillName), and the fields below. C4: 0x1C58
// bytes (the base is 0x1C40, C3: 0x1A4C over 0x1A34); the constructor is 0x10115A60 (NItemEntry's 0x1001F7F0, then
// bVisible = bFadeOut = 1, RemainTime = 0, Alpha = 255).
struct NAbnormalEntry : public NItemEntry
{
    INT IconX;                       // +0x1C40 (name ours) the icon's place, left by each paint (the tool tip uses it)
    INT IconY;                       // +0x1C44 (name ours)
    INT RemainTime;                  // +0x1C48 seconds (the timer counts it down)
    INT Alpha;                       // +0x1C4C (255) the icon's alpha: in its last 30 seconds it goes up and down by 25
                                     //   every 100 ms (TIMER_AbnormalFade)
    UBOOL bVisible;                  // +0x1C50 (1) the icon is drawn
    UBOOL bFadeOut;                  // +0x1C54 (1) the direction of the fade

    __declspec(noinline) NAbnormalEntry();   // 0x10115A60: IconX, IconY are left as allocated (as in the stock constructor)
};
NW_ASSERT_OFFSET(NAbnormalEntry, IconX, 0x1C40);
NW_ASSERT_OFFSET(NAbnormalEntry, RemainTime, 0x1C48);
NW_ASSERT_OFFSET(NAbnormalEntry, Alpha, 0x1C4C);
NW_ASSERT_OFFSET(NAbnormalEntry, bFadeOut, 0x1C54);
NW_ASSERT_SIZE(NAbnormalEntry, 0x1C58);

// 0x1006a9b0 (anonymous in the db, name ours; ret 8, `this` is not used; defined in NCBroadcastObserverBuffWnd.cpp) - whether a skill is a
// debuff: its record in the skill table has the debuff flag set. Called on the console (the buff windows draw the debuff frame by it)
struct NConsoleWndSkillExt
{
    __declspec(noinline) INT IsDebuffSkill(INT SkillID, INT Level);
};

enum { TIMER_AbnormalStatus = 0xF9, TIMER_AbnormalFade = 0xE9 };

class NCAbnormalStatusWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCAbnormalStatusWnd, NCWnd, 0, 0x1030B5A0)

    TArray<NAbnormalEntry*> Statuses; // +0xE8 the buffs, 31 at most
    TArray<NAbnormalEntry*> Statuses2; // +0xF4 (new in C3, name ours) a second list the paint draws after the first
                                     //   (its count is +0xD8; OnPaint, OnDrawToolTip and vslot 68 read it)
    NAbnormalEntry* ExtraStatus;     // +0x100 (new in C3, name ours) one more icon, drawn after the lists and timed like them
    INT TimerID;                     // +0x104 SetTimer(0xF9, 1000): the time runs down
    INT FadeTimerID;                 // +0x108 SetTimer(0xE9, 100): the blink of the last ten seconds
    UTexture* BackTex;               // +0x10C "L2UI.EtcWndBack.AbnormalBack" under each icon
    UTexture* DebuffTex;             // +0x110 "L2UI_CH3.PlayerStatusWnd.debuff" over the icons of a debuff (C3)
    UTexture* ToolTipTex;            // +0x114 "L2UI.tooltip.mini-tool01" (not drawn here)
    UTexture* TipTopTex;             // +0x118 "L2UI.Tooltip.Tool02" (C1: the box of the tool tip; C2 draws it with the skin)
    UTexture* TipMidTex;             // +0x11C "L2UI.Tooltip.Tool01"
    UTexture* TipBottomTex;          // +0x120 "L2UI.Tooltip.Tool03"
    INT IconSize;                    // +0x124 constructor argument (24); HitTest uses it
    INT HoverIndex;                  // +0x128 icon under the mouse, -1 = none

    NCAbnormalStatusWnd(INT InIconSize = 24);                             // 0x100d8560 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCAbnormalStatusWnd                                          // 0x100d85f0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnTimer(INT InTimerID);                              // 36 0x1010eef0
    virtual INT OnCreate();                                          // 37 0x10112c10
    virtual INT OnDestroy();                                         // 38 0x10117970
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1010e560
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10116640 (the db calls it OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x1010ee50

    // names ours
    INT HitTest(INT X, INT Y);                                       // 0x1010d1c0
    INT AddStatus(FAbnormalStatus* Status);                          // 0x10115aa0 0 when full (31 held)
    INT AddStatus2(FAbnormalStatus* Status);                         // 0x10115cb0 (C4: the second list's; 0 when 10 are held)
    INT SetExtraStatus(FAbnormalStatus* Status);                     // 0x10117a70 the extra icon (+0x100); an empty icon name clears it
    void ClearStatus();                                              // 0x10116a10
    void ClearStatus2();                                             // 0x10116960 the second list gone
    void ClearExtraStatus();                                         // 0x101169d0 the extra icon gone
    virtual void UpdateLayout();                                     // 74 0x1010d220 (only this client has it) the size from the icons held
};
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, Statuses, 0xE8);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, Statuses2, 0xF4);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, ExtraStatus, 0x100);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, TimerID, 0x104);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, BackTex, 0x10C);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, TipTopTex, 0x118);
NW_ASSERT_OFFSET(NCAbnormalStatusWnd, HoverIndex, 0x128);
NW_ASSERT_SIZE(NCAbnormalStatusWnd, 0x12C);

#pragma pack(pop)
