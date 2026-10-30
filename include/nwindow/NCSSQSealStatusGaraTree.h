#pragma once
// NCSSQSealStatusGaraTree - the Seven Signs seal status list (UClass "CSSQSealStatusGaraTree", 0x228; new in C3):
// the same shape with two more bars (the dusk's and the dawn's: "ssq_bar1x", "ssq_bar2x"); its DrawItem is its own.
// NCSSQSealStatusListWnd holds one inside an NCSSQSealStatusGaraTreeCtrl.

#include "nwindow/NCGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCSSQSealStatusGaraTree : public NCGaraTree
{
    NW_DECLARE_CLASS(NCSSQSealStatusGaraTree, NCGaraTree, 0, 0x10304340)

    UTexture* Panel1Tex;             // +0x20C "L2UI_CH3.QUESTWND.QUESTWNDPANEL1" (names ours)
    UTexture* Panel2Tex;             // +0x210 "...PANEL2"   // C2: offset derived, no evidence
    UTexture* Panel3Tex;             // +0x214 "...PANEL3"   // C2: offset derived, no evidence
    UTexture* LineTex;               // +0x218 "L2UI_CH3.Etc.TextBackline"   // C2: offset derived, no evidence
    UTexture* SelectTex;             // +0x21C "L2UI_CH3.ListCtrl.TextSelect"   // C2: offset derived, no evidence
    UTexture* SelectTex2;            // +0x220 "L2UI_CH3.ListCtrl.TextSelect2"   // C2: offset derived, no evidence
    UTexture* Bar21Tex;              // +0x224 "L2UI_CH3.SSQWnd.ssq_bar21"   // C2: offset derived, no evidence
    UTexture* Bar22Tex;              // +0x228 "...ssq_bar22"   // C2: offset derived, no evidence
    UTexture* Bar2BackTex;           // +0x22C "...ssq_bar2back"   // C2: offset derived, no evidence
    UTexture* Bar11Tex;              // +0x230 "...ssq_bar11"   // C2: offset derived, no evidence
    UTexture* Bar12Tex;              // +0x234 "...ssq_bar12"   // C2: offset derived, no evidence
    UTexture* Bar1BackTex;           // +0x238 "...ssq_bar1back"
    UTexture* BarLineTex;            // +0x23C "...ssq_barline"
    BYTE _Unknown220[0x228 - 0x220]; // +0x240 never touched   // C2: offset derived, no evidence

    NCSSQSealStatusGaraTree();                                       // 0x100ff890
    // ~NCSSQSealStatusGaraTree                                      // 0x100ff8e0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e9f60 (folded with NCExButton's)
    virtual INT OnCreate();                                          // 37 0x100fe3c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fe570
    virtual INT DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y);   // 78 0x100fe660
};
NW_ASSERT_OFFSET(NCSSQSealStatusGaraTree, Panel1Tex, 0x20C);
NW_ASSERT_OFFSET(NCSSQSealStatusGaraTree, Bar21Tex, 0x224);
NW_ASSERT_OFFSET(NCSSQSealStatusGaraTree, BarLineTex, 0x23C);
NW_ASSERT_SIZE(NCSSQSealStatusGaraTree, 0x248);

#pragma pack(pop)
