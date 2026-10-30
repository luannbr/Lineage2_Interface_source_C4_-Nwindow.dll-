#pragma once
// NCSkillItemWnd - a skill grid of the skill window (UClass "CSkillItemWnd", 0x134; the Active
// and Passive lists of NCSkillWnd, 6 x 7 icons of 32 pixels). Each skill has a cool time icon
// drawn over it (CoolTimes, one per item, same index); the active ones get the icon frame and,
// while pressed, the click frame, and a toggle skill whose effect is on the player the toggle
// animation. Released over the same icon a skill is used (Ctrl forces, Shift stays in place);
// dropped on a shortcut bar slot it is registered there. The tool tip is always the box: name
// and type, "MP cost" and "Range" when the skill has them, description.
// NCGMSkillItemWnd, the GM view of a player's skills, uses nothing on release.

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCCoolTimeIcon;

class NCSkillItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCSkillItemWnd, NCItemWnd, 0, 0x102E3AA8)

    UTexture* IconBackTex;             // +0x168 "l2ui.nwindow.icon_back" frame of an active skill
    UTexture* IconClickTex;            // +0x16C "l2ui.nwindow.icon_click" while pressed
    UTexture* ToggleTex;               // +0x170 "L2_SkillTime.ToggleEffect.ToggleEffect001"   // C2: offset derived, no evidence
    UBOOL bPressed;                    // +0x174 the left button is down on the grid
    TArray<NCCoolTimeIcon*> CoolTimes; // +0x178 one per item (the console starts them)

    NCSkillItemWnd();                                                // 0x100ae9d0
    // C3: the cell gaps are arguments too (the skill windows pass 5 and 3), ret 0x18
    NCSkillItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100aea70
    // ~NCSkillItemWnd                                               // 0x1008f400

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790
    virtual INT OnDestroy();                                         // 38 0x100ae730
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ad8a0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100aeb30
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100adc10
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100ad7b0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100adde0
    virtual void Clear();                                            // 78 0x100ae920

    // names ours
    INT AddSkill(NItemInfo* Info);                                   // 0x100ae800 no limit
    // the cool time of a skill's icon (the console reads it for the shortcut bar): the total and what is left, 0 and 0 when
    // the skill is not here
    void GetCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining);  // 0x100ae140 (ret 0xC)
};
NW_ASSERT_OFFSET(NCSkillItemWnd, IconBackTex, 0x168);
NW_ASSERT_OFFSET(NCSkillItemWnd, bPressed, 0x174);
NW_ASSERT_OFFSET(NCSkillItemWnd, CoolTimes, 0x178);
NW_ASSERT_SIZE(NCSkillItemWnd, 0x184);

// NCGMSkillItemWnd - the grids of NCGMSkillWnd (UClass "CGMSkillItemWnd", 0x134): a click
// only selects.
class NCGMSkillItemWnd : public NCSkillItemWnd
{
    NW_DECLARE_CLASS(NCGMSkillItemWnd, NCSkillItemWnd, 0, 0x102D55D8)

    NCGMSkillItemWnd();                                              // 0x1008fc30
    NCGMSkillItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x1008fcd0
    // ~NCGMSkillItemWnd                                             // 0x1008fc80

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10089140
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10089090 (folded)
};
NW_ASSERT_SIZE(NCGMSkillItemWnd, 0x184);

#pragma pack(pop)