#pragma once
// NCMagicSkillWnd - the skill window (UClass "CMagicSkillWnd", 0x9C; a page of the character
// window, NCMainWnd): the NCSkillWnd lists on "L2UI.SkillWnd.skillBack". Shown, it empties
// the lists and asks the server for the skill list; hidden, it empties them.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCSkillWnd;
class NCGMSkillWnd;

// NWM_COMMAND ids: the skill list (both windows), the GM window's title bar
enum { NID_MAGICSKILL_LIST = 0x1F01, NID_GMMAGICSKILL_FRAME = 0xF0F117 };

class NCMagicSkillWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMagicSkillWnd, NCWnd, 0, 0x102E3578)

    UTexture* BackTex;                     // +0xE8 "L2UI.SkillWnd.skillBack"   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NCSkillWnd* SkillWnd;                  // +0xEC

    NCMagicSkillWnd();                                               // 0x100aef40
    // ~NCMagicSkillWnd                                              // 0x10091670 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100aefe0
    virtual INT OnDestroy();                                         // 38 0x100ae350 (NCPetActionWnd's)
    virtual void ShowWindow();                                       // 48 0x100ae4a0
    virtual void HideWindow();                                       // 49 0x100ae520
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ae400

    // names ours
    __declspec(noinline) void ClearSkills();                                              // 0x100ad890
    void ReloadSkillTexts();                                         // 0x100ae530 new language
};
NW_ASSERT_OFFSET(NCMagicSkillWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMagicSkillWnd, SkillWnd, 0xEC);
NW_ASSERT_SIZE(NCMagicSkillWnd, 0xF0);

// NCGMMagicSkillWnd - a GM's view of another player's skills (UClass "CGMMagicSkillWnd", 0x9C;
// desktop window): an NCGMSkillWnd under a title bar, on the same background. Hidden, it
// empties the lists; the console fills them.
class NCGMMagicSkillWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMMagicSkillWnd, NCWnd, 0, 0x102D6038)

    NCGMSkillWnd* SkillWnd;                // +0xE8
    UTexture* BackTex;                     // +0xEC "L2UI.SkillWnd.skillBack"

    NCGMMagicSkillWnd();                                             // 0x10090020
    // ~NCGMMagicSkillWnd                                            // 0x10074730 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100900c0
    virtual INT OnDestroy();                                         // 38 0x1008aa60
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual void HideWindow();                                       // 49 0x1008abb0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008ab10

    // names ours
    __declspec(noinline) void ClearSkills();                                              // 0x100891c0
};
NW_ASSERT_OFFSET(NCGMMagicSkillWnd, SkillWnd, 0xE8);
NW_ASSERT_OFFSET(NCGMMagicSkillWnd, BackTex, 0xEC);
NW_ASSERT_SIZE(NCGMMagicSkillWnd, 0xF0);

#pragma pack(pop)