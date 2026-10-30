#pragma once
// NCSSQMainEventGaraTree - the Seven Signs event list (UClass "CSSQMainEventGaraTree", 0x20C; new in C3): the
// quest tree's shape with the Seven Signs panels; its DrawItem is its own. NCSSQMainEventWnd holds one inside an
// NCSSQMainEventGaraTreeCtrl.

#include "nwindow/NCGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCSSQMainEventGaraTree : public NCGaraTree
{
    NW_DECLARE_CLASS(NCSSQMainEventGaraTree, NCGaraTree, 0, 0x103038D8)

    UTexture* Panel1Tex;             // +0x20C "L2UI_CH3.QUESTWND.QUESTWNDPANEL1" (names ours)
    UTexture* Panel2Tex;             // +0x210 "...PANEL2"   // C2: offset derived, no evidence
    UTexture* Panel3Tex;             // +0x214 "...PANEL3"   // C2: offset derived, no evidence
    UTexture* LineTex;               // +0x218 "L2UI_CH3.Etc.TextBackline"   // C2: offset derived, no evidence
    UTexture* SelectTex;             // +0x21C "L2UI_CH3.ListCtrl.TextSelect"
    UTexture* SelectTex2;            // +0x220 "L2UI_CH3.ListCtrl.TextSelect2"
    BYTE _Unknown204[0x20C - 0x204]; // +0x224 never touched   // C2: offset derived, no evidence

    NCSSQMainEventGaraTree();                                        // 0x100fdf40
    // ~NCSSQMainEventGaraTree                                       // 0x100fdf90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e9f60 (folded with NCExButton's)
    virtual INT OnCreate();                                          // 37 0x100fcb40
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fcc60
    virtual INT DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y);   // 78 0x100fcd50
};
NW_ASSERT_OFFSET(NCSSQMainEventGaraTree, Panel1Tex, 0x20C);
NW_ASSERT_OFFSET(NCSSQMainEventGaraTree, SelectTex2, 0x220);
NW_ASSERT_SIZE(NCSSQMainEventGaraTree, 0x22C);

#pragma pack(pop)
