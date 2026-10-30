#pragma once
// NCManorInfoDownWnd - the base of the manor information pages (new in C4; UClass "CManorInfoDownWnd", 0xF0): the back
// picture and a list. NCManorCropInfoWnd, NCManorSeedInfoWnd and NCManorDefaultInfoWnd derive from it in the stock code (they
// run its OnCreate / OnPaint / OnDestroy first); their headers keep NCWnd as the base and declare the two inherited fields,
// so the shared bodies are called by address there. Nothing creates this class itself.

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// the id of the list (name ours)
enum
{
    NID_MANORINFODOWN_LIST = (INT)0xF0000001,
};

class NCManorInfoDownWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorInfoDownWnd, NCWnd, 0, 0x10316BE0)

    NCListCtrl* ListCtrl;            // +0xE8 id 0xF0000001, at (7, 42), 586 x 199, up to 1000 rows
    UTexture* BackTex;               // +0xEC "L2UI_CH3.Manor.manor1_back2"

    NCManorInfoDownWnd();                                            // 0x101334b0
    // ~NCManorInfoDownWnd                                           // 0x10133430

    // WindowProc (25) is NControl's: this class does not override it
    virtual INT OnCreate();                                          // 37 0x10136960
    virtual INT OnDestroy();                                         // 38 0x101367b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10136860
    virtual INT NewVirtual74(INT Row);                               // 74 0x100023c0 (shared stub, ret 4: the row of the list's command, unused)
};
// the derived pages' headers keep NCWnd as the base and declare the two inherited fields, which sit where this class has them:
// the shared bodies are called as this class' own, not virtually
#define MANOR_DOWN_BASE() (reinterpret_cast<NCManorInfoDownWnd*>(this))

NW_ASSERT_OFFSET(NCManorInfoDownWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorInfoDownWnd, BackTex, 0xEC);
NW_ASSERT_SIZE(NCManorInfoDownWnd, 0xF0);

#pragma pack(pop)
