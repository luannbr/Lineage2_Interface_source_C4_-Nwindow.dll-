#pragma once
// NCTrainListBox - the skill list of the trainer window (UClass "CTrainListBox", 0x1F4). Rows
// are 32 px high plus LineGap (4): the skill icon, its name and, below it, "Lv <level>" and
// "Required SP : <sp>". The rows are NTrainEntry records kept in Entries (NCListBox::Items is
// not used) and the scroll range follows their count. Clicking a row selects it and reports
// its index to the parent (NWM_COMMAND, like NCListBox).

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// one skill row (names ours; 0x14A8 bytes in C4, 0x149C in C3: three more fields at the end, see AddEntry 0x1011DE30 and the
// stock constructor 0x1006E8C0). The window fills one on the stack and the list keeps a copy (AddEntry); GetSelectedEntry
// copies the selected one back.
struct NTrainEntry
{
    TCHAR Name[256];           // +0x000 (C1: 128 characters, every offset below was 0x600 less)
    TCHAR IconName[256];       // +0x200 texture looked up by AddEntry
    TCHAR Desc[2048];          // +0x400
    TCHAR UnknownA00[64];      // +0x1400
    UTexture* Icon;            // +0x1480 NWindow.BlackTexture when not found
    INT SkillID;               // +0x1484 sent with Level when a row is picked
    INT Level;                 // +0x1488 <= 0: SysString 371 ("Unacquired state")
    INT UnknownA8C;            // +0x148C never copied
    INT SP;                    // +0x1490 required SP
    INT UnknownA94;            // +0x1494
    INT UnknownA98;            // +0x1498
    INT Unknown149C;           // +0x149C (0) new in C4
    INT bEnchanted;            // +0x14A0 (0) new in C4 (name ours): the row is an enchanted route of the skill: its name (FL2MagicSkillData
                               //   +0x50) is drawn in place of "Lv <level>" and "Required SP"
    INT Unknown14A4;           // +0x14A4 (-1) new in C4

    NTrainEntry()
    {
        Name[0] = 0;
        IconName[0] = 0;
        Desc[0] = 0;
        UnknownA00[0] = 0;
        Icon = NULL;
        SkillID = -1;
        Level = -1;
        UnknownA8C = -1;
        SP = 0;
        UnknownA94 = 0;
        Unknown149C = 0;
        Unknown14A4 = -1;
        bEnchanted = 0;
        UnknownA98 = -1;
    }

    // inlined in AddEntry and GetSelectedEntry: empty strings and a NULL icon are not copied
    // (UnknownA8C never is)
    __forceinline void Copy(const NTrainEntry* Src)
    {
        if (Src && Src->Name[0])
            wcscpy(Name, Src->Name);
        if (Src->IconName && Src->IconName[0])
            wcscpy(IconName, Src->IconName);
        if (Src->Desc && Src->Desc[0])
            wcscpy(Desc, Src->Desc);
        if (Src->UnknownA00 && Src->UnknownA00[0])
            wcscpy(UnknownA00, Src->UnknownA00);
        if (Src->Icon)
            Icon = Src->Icon;
        SkillID = Src->SkillID;
        Level = Src->Level;
        SP = Src->SP;
        Unknown149C = Src->Unknown149C;
        Unknown14A4 = Src->Unknown14A4;
        bEnchanted = Src->bEnchanted;
        UnknownA94 = Src->UnknownA94;
        UnknownA98 = Src->UnknownA98;
    }
};
NW_ASSERT_OFFSET(NTrainEntry, Icon, 0x1480);
NW_ASSERT_OFFSET(NTrainEntry, SP, 0x1490);
NW_ASSERT_OFFSET(NTrainEntry, bEnchanted, 0x14A0);
NW_ASSERT_SIZE(NTrainEntry, 0x14A8);

class NCTrainListBox : public NCListBox
{
    NW_DECLARE_CLASS(NCTrainListBox, NCListBox, 0, 0x1030DA08)

    TArray<NTrainEntry*> Entries;    // +0x240
    UTexture* OutlineDownTex;        // +0x24C (new in C3, name ours) "l2ui_ch3.InventoryWnd.Inventory_OutLine_down": the frame under the icon
    UTexture* OutlineTex;            // +0x250 (new in C3, name ours) "l2ui_ch3.InventoryWnd.Inventory_OutLine"

    NCTrainListBox();                                                // 0x1011e300
    NCTrainListBox(INT InMaxLines, INT InVisibleLines, INT Unused);  // 0x100bcc00 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCTrainListBox                                               // 0x100bcba0 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)

    // the stock bodies of 35, 36, 56, 63, 65 and UpdateEntryScrollBar are shared with
    // NCVIPShopItemListBox (identical code)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890 (NCDebugWnd's)
    virtual INT OnCreate();                                          // 37 0x1011b740
    virtual INT OnDestroy();                                         // 38 0x1011ddc0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011c580
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1011b870
    virtual INT HitTest(INT X, INT Y);                               // 75 0x1011b790
    virtual void Clear();                                            // 77 0x100f1b60

    // names ours
    void UpdateEntryScrollBar();                                     // 0x100c1110 (C1 only: no C2 match) (C1 only: no C2 match) (shared with NCRecipeListBox's)
    void AddEntry(const NTrainEntry* Src);                           // 0x1011de30
    UBOOL GetSelectedEntry(NTrainEntry* Out);                        // 0x1011cbf0
};
NW_ASSERT_OFFSET(NCTrainListBox, Entries, 0x240);
NW_ASSERT_OFFSET(NCTrainListBox, OutlineDownTex, 0x24C);
NW_ASSERT_SIZE(NCTrainListBox, 0x254);

#pragma pack(pop)