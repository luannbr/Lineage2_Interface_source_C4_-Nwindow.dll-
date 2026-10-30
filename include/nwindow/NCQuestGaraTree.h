#pragma once
// NCQuestGaraTree - the quest list of the main window's quest tab (UClass "CQuestGaraTree", 0x228; new in C3): an
// NCGaraTree whose entries are the quests (the groups) and their steps, drawn on the three panels of the quest
// window with the five info icons, the selected one on the "TextSelect" bar. DrawItem is its own (the stock one is
// 2851 bytes).

#include "nwindow/NCGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCQuestGaraTree : public NCGaraTree
{
    NW_DECLARE_CLASS(NCQuestGaraTree, NCGaraTree, 0, 0x102F9CE8)

    UTexture* Panel1Tex;             // +0x20C "L2UI_CH3.QUESTWND.QUESTWNDPANEL1" (name ours)
    UTexture* Panel2Tex;             // +0x210 "...PANEL2"   // C2: offset derived, no evidence
    UTexture* Panel3Tex;             // +0x214 "...PANEL3"   // C2: offset derived, no evidence
    UTexture* OutlineTex;            // +0x218 "L2UI_CH3.Etc.menu_outline"   // C2: offset derived, no evidence
    UTexture* SelectTex;             // +0x21C "L2UI_CH3.ListCtrl.TextSelect"
    UTexture* SelectTex2;            // +0x220 "L2UI_CH3.ListCtrl.TextSelect2"
    NCGaraTVItem* SelectedItem;      // +0x224 the entry that was clicked last (NCGaraTVItem::OnLButtonDown sets it)
    UTexture* InfoIconTex[5];        // +0x228 "L2UI_CH3.QUESTWND.QuestWndInfoIcon_1" .. "_5"   // C2: offset derived, no evidence
    BYTE Unknown21C;                 // +0x23C (0)
    BYTE _Unknown21D[0x228 - 0x21D]; // +0x23D never touched   // C2: offset derived, no evidence

    NCQuestGaraTree();                                               // 0x100ed080
    // ~NCQuestGaraTree                                              // 0x100ed0d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e9f60 (folded with NCExButton's)
    virtual INT OnCreate();                                          // 37 0x100e9f80
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ea540
    virtual INT DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y);   // 78 0x100ea630
};
NW_ASSERT_OFFSET(NCQuestGaraTree, Panel1Tex, 0x20C);
NW_ASSERT_OFFSET(NCQuestGaraTree, SelectedItem, 0x224);
NW_ASSERT_OFFSET(NCQuestGaraTree, InfoIconTex, 0x228);
NW_ASSERT_OFFSET(NCQuestGaraTree, Unknown21C, 0x23C);
NW_ASSERT_SIZE(NCQuestGaraTree, 0x248);

#pragma pack(pop)
