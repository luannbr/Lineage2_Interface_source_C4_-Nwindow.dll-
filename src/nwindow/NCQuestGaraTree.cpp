/*=============================================================================
    NCQuestGaraTree.cpp - the quest tree. C3 stock: 0x100DA190 .. 0x100DA264 (OnCreate), 0x100DA440 .. 0x100DA4FC
    (paint), 0x100DA530 .. 0x100DB06B (DrawItem), 0x100DC070 .. 0x100DC10F (constructor, destructor).
=============================================================================*/
#include "nwindow/NCQuestGaraTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x100ed080 - NCGaraTree's, then the flag and the selected entry empty
NCQuestGaraTree::NCQuestGaraTree()
{
    Unknown21C = 0;
    SelectedItem = NULL;
}
NW_HOOK_CTOR(0x100ed080, NCQuestGaraTree)

// 0x100ed0d0
NW_HOOK_DTOR(0x100ed0d0, NCQuestGaraTree)

// 0x10041760 (vslot 25) - the stock body is the one the linker shared with NCExButton's: NControl's, as a flag
INT NCQuestGaraTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100e9f80 (vslot 37) - NControl's, then the panels, the five info icons, the outline and the two halves of the
// selection bar
INT NCQuestGaraTree::OnCreate()
{
    NControl::OnCreate();
    Panel1Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL1"));
    Panel2Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL2"));
    Panel3Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL3"));
    InfoIconTex[0] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_1"));
    InfoIconTex[1] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_2"));
    InfoIconTex[2] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_3"));
    InfoIconTex[3] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_4"));
    InfoIconTex[4] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_5"));
    OutlineTex = FindTexture(TEXT("L2UI_CH3.Etc.menu_outline"));
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    return 1;
}
NW_HOOK_METHOD(0x100e9f80, NCQuestGaraTree, OnCreate)

// 0x100ea540 (vslot 62) - NCGaraTree's paint, the clip 274 pixels high and the entries starting at (15, 5)
INT NCQuestGaraTree::OnPaint(NCGDevice* Device)
{
    guard(NCQuestGaraTree::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY + ScrollOffset, (INT)Width, 274);
    ContentHeight = DrawItem(Device, Root, 15, 5);
    SetWindowSize(Width, (FLOAT)ContentHeight);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ea540, NCQuestGaraTree, OnPaint)

// sysstring lines of the rows (names ours)
enum
{
    STR_QuestLevel = 922,            // "Level" in "(Level:10~20)"
    STR_Unlimited = 858,
    STR_Pieces = 859,
    STR_AnyLevel = 866,
    STR_CountText = 898,             // what the counts show instead of the owned number (bTextCount)
};

// row kinds (NGaraTVItemData::Kind; names ours)
enum { QTREE_Group = 1, QTREE_Quest = 2, QTREE_Details = 3 };

// how many characters the FString holds, as the stock tests it (the array's count, not the length)
static FORCEINLINE INT StringNum(const FString& S)
{
    return ((const TArray<TCHAR>*)&S)->Num();
}

// The quest table (GL2GameData + 0x3D28: 2000 records of 0xA0 bytes, FL2QuestData) looked up by quest id and step. The two
// stock lookups below walk it with `this` (the console) unused; they are new in C4.
static FORCEINLINE const BYTE* FindQuestRecord(INT QuestID, INT Step)
{
    const BYTE* Record = (const BYTE*)&GL2GameData + 0x3D28;
    for (INT i = 0; i < 2000; i++, Record += 0xA0)
    {
        const FL2QuestData* Quest = (const FL2QuestData*)Record;
        if (Quest->bValid && Quest->QuestID == QuestID && Quest->Step == Step)
            return Record;
    }
    return NULL;
}

// 0x10067450 (ret 8; name ours) - the "journal_disp" column (record + 0x24) of the quest step: the count of a quest row is
// shown as the text instead of the numbers only when the quest step has it
static bool __fastcall QuestStepHasJournalDisp(void* Unused, void* Edx, INT QuestID, INT Step)
{
    const BYTE* Record = FindQuestRecord(QuestID, Step);
    return Record != NULL && *(const INT*)(Record + 0x24) != 0;
}
NW_HOOK_FUNC(0x10067450, QuestStepHasJournalDisp)

// 0x100674b0 (ret 8; name ours) - the same walk, the column at record + 0x4C (the details' count rows)
static bool __fastcall QuestStepHasColumn4C(void* Unused, void* Edx, INT QuestID, INT Step)
{
    const BYTE* Record = FindQuestRecord(QuestID, Step);
    return Record != NULL && *(const INT*)(Record + 0x4C) != 0;
}
NW_HOOK_FUNC(0x100674b0, QuestStepHasColumn4C)

// 0x100ea630 (vslot 78) - the three kinds of row the console makes, put at (X, Y). A group (kind 1) is its picture,
// its name and its level range, with two small icons by its name; a quest (kind 2) its picture and name, a mark when
// it has a place on the map and its "done" text, and it becomes the tree's selected row; the details (kind 3) a
// panel of three pieces with the text and, for each item the quest wants, its picture, its name and how many the
// player owns of how many are needed. The children are laid out under the row the way NCGaraTree does, indented by
// half the picture's width, and hidden when the row is closed. C4: the count text replaces the numbers only when the
// quest step has the flag too (the two lookups above; the step is the row's IconSet)
INT NCQuestGaraTree::DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y)
{
    if (!Item)
        return 0;
    INT Used = 0;
    Item->CopyW = X;
    Item->CopyH = Y;
    Item->SetPosition(X, Y, Item->Data.Width + (Item->Children.Num() > 0 ? 16 : 0), Item->Data.Height);
    INT Kind = Item->Data.Kind;
    if (Kind == QTREE_Group)
    {
        if (Item->bExpanded == 0)
        {
            Device->DrawTexture(X, Y, Item->Data.IconW, Item->Data.IconH, 0, 0, Item->Data.IconW, Item->Data.IconH,
                                Item->Data.Icon, 0xFF, 1);
        }
        else
        {
            Device->DrawTexture(X, Y, Item->Data.IconW, Item->Data.IconH, 0, 0, Item->Data.IconW, Item->Data.IconH,
                                Item->Data.ExpandedIcon, 0xFF, 1);
            Device->DrawStretchedTex(Item->Data.IconW + X, Y + 1, (INT)(Width - 47.0f - 2.0f), 13, 0, 0, 16, 13,
                                     SelectTex, 0xFF);
            Device->DrawStretchedTex((INT)((FLOAT)(Item->Data.IconW + X) + Width - 47.0f - 2.0f), Y + 1, 32, 13, 0, 0,
                                     32, 13, SelectTex2, 0xFF);
        }
        Device->DrawText(Item->Data.IconW + X + 5, Y + 2, 0xFFDCDCDC, Item->Data.Text, (INT)Width, 0, 0, L2FONT_Normal,
                         0, 0, 0, 0, 0, 0, 0);
        SIZE TextSize;
        NCGDevice::GetTextExtent(&TextSize, Item->Data.Text, (INT)Width, 0, 0, 0, 0);
        UTexture* IconA;
        UTexture* IconB;
        switch (Item->Data.IconSet)
        {
        case 0:
            IconA = InfoIconTex[0];
            IconB = InfoIconTex[3];
            break;
        case 1:
            IconA = InfoIconTex[1];
            IconB = InfoIconTex[3];
            break;
        case 2:
            IconA = InfoIconTex[0];
            IconB = InfoIconTex[2];
            break;
        case 3:
            IconA = InfoIconTex[1];
            IconB = InfoIconTex[2];
            break;
        default:
            IconA = NULL;
            IconB = NULL;
            break;
        }
        if (Item->Data.IconSet >= 0)
        {
            Device->DrawTexture(Item->Data.IconW + TextSize.cx + X + 10, Y, 11, 11, 0, 0, 11, 11, IconB, 0xFF, 1);
            Device->DrawTexture(Item->Data.IconW + TextSize.cx + X + 21, Y, 11, 11, 0, 0, 11, 11, IconA, 0xFF, 1);
        }
        TCHAR RangeText[512];
        if (Item->Data.LevelMin > 0)
        {
            if (Item->Data.LevelMax > 0)
                appSprintf(RangeText, TEXT("(%s:%d~%d)"), *GL2GameData.SysString[STR_QuestLevel], Item->Data.LevelMin,
                           Item->Data.LevelMax);
            else
                appSprintf(RangeText, TEXT("(%s:%d %s)"), *GL2GameData.SysString[STR_QuestLevel], Item->Data.LevelMin,
                           *GL2GameData.SysString[STR_Pieces]);
        }
        else
        {
            appSprintf(RangeText, TEXT("(%s:%s)"), *GL2GameData.SysString[STR_QuestLevel],
                       *GL2GameData.SysString[STR_AnyLevel]);
        }
        Device->DrawText(Item->Data.IconW + X + 5, TextSize.cy + Y + 2, 0xFFB09B79, RangeText, (INT)Width, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Used = Item->Data.IconH + TextSize.cy + 7;
    }
    else if (Kind == QTREE_Quest)
    {
        UTexture* Picture = Item->bExpanded == 0 ? Item->Data.Icon : Item->Data.ExpandedIcon;
        Device->DrawTexture(X, Y, Item->Data.IconW, Item->Data.IconH, 0, 0, Item->Data.IconW, Item->Data.IconH, Picture,
                            0xFF, 1);
        Device->DrawText(Item->Data.IconW + X + 5, Y, 0xFFDCDCDC, Item->Data.Text, (INT)Width, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0, 0);
        SIZE TextSize;
        NCGDevice::GetTextExtent(&TextSize, Item->Data.Text, 0, 0, 0, 0, 0);
        UBOOL bMark = 0;
        if (StringNum(Item->Data.MapPlace) != 0 && StringNum(Item->Data.MapPlace) - 1 > 0)
        {
            Device->DrawTexture(Item->Data.IconW + TextSize.cx + X + 10, Y, 11, 11, 0, 0, 11, 11, InfoIconTex[4],
                                0xFF, 1);
            bMark = 1;
        }
        UBOOL bCountOn = QuestStepHasJournalDisp(NULL, NULL, Item->Data.QuestID, Item->Data.IconSet);
        if (Item->Data.bTextCount != 0 && bCountOn)
        {
            if (bMark)
                Device->DrawText(Item->Data.IconW + TextSize.cx + X + 26, Y, 0xFFB09B79,
                                 *GL2GameData.SysString[STR_CountText], (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                                 0, 0);
            else
                Device->DrawText(Item->Data.IconW + TextSize.cx + X + 10, Y, 0xFFB09B79,
                                 *GL2GameData.SysString[STR_CountText], (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                                 0, 0);
        }
        Used = Item->Data.IconH + 5;
        SelectedItem = Item;
    }
    else if (Kind == QTREE_Details)
    {
        FString Details;
        GConsole->ConvertNewLines(Item->Data.Text2, Details);
        SIZE TextSize;
        NCGDevice::GetTextExtent(&TextSize, *Details, (INT)(Width - (FLOAT)X - 5.0f), 0, 1, 0, 0);
        INT PanelHeight = (Item->Data.IconH + 5) * Item->Data.Count + TextSize.cy + 10;
        Device->DrawTexture(X - 5, Y - 2, 211, 8, 0, 0, 211, 8, Panel1Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, Y + 6, 211, PanelHeight - 16, 0, 0, 211, 8, Panel2Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, PanelHeight + Y - 10, 211, 8, 0, 0, 211, 8, Panel3Tex, 0xFF, 1);
        Device->DrawText(X, Y, 0xFF8C8C8C, *Details, (INT)(Width - (FLOAT)X - 5.0f), 0, 1, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);
        UBOOL bCountOn = QuestStepHasColumn4C(NULL, NULL, Item->Data.QuestID, Item->Data.IconSet);
        TCHAR CountText[256];
        for (INT i = 0; i < Item->Data.Count; i++)
        {
            Device->DrawTexture(X, (Item->Data.IconH + 5) * i + TextSize.cy + Y + 5, Item->Data.IconW, Item->Data.IconH,
                                0, 0, Item->Data.IconW, Item->Data.IconH, Item->Data.ItemIcon[i], 0xFF, 1);
            Device->DrawTexture(X - 1, (Item->Data.IconH + 5) * i + TextSize.cy + Y + 4, 34, 34, 0, 0, 34, 34,
                                OutlineTex, 0xFF, 1);
            Device->DrawText(Item->Data.IconW + X + 5, (Item->Data.IconH + 5) * i + TextSize.cy + Y + 5, 0xFFB09B79,
                             *Item->Data.ItemName[i], (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            INT Needed = Item->Data.ItemNeeded[i];
            if (Needed > 0)
            {
                if (Item->Data.bTextCount && bCountOn)
                    appSprintf(CountText, TEXT("(%s/%d)"), *GL2GameData.SysString[STR_CountText], Needed);
                else
                    appSprintf(CountText, TEXT("(%d/%d)"), Item->Data.ItemOwned[i], Needed);
            }
            else if (Needed == 0)
            {
                if (Item->Data.bTextCount && bCountOn)
                    appSprintf(CountText, TEXT("(%s/%s)"), *GL2GameData.SysString[STR_CountText],
                               *GL2GameData.SysString[STR_Unlimited]);
                else
                    appSprintf(CountText, TEXT("(%d/%s)"), Item->Data.ItemOwned[i],
                               *GL2GameData.SysString[STR_Unlimited]);
            }
            else
            {
                if (Item->Data.bTextCount && bCountOn)
                    appSprintf(CountText, TEXT("(%s/%d%s)"), *GL2GameData.SysString[STR_CountText], -Needed,
                               *GL2GameData.SysString[STR_Pieces]);
                else
                    appSprintf(CountText, TEXT("(%d/%d%s)"), Item->Data.ItemOwned[i], -Needed,
                               *GL2GameData.SysString[STR_Pieces]);
            }
            SIZE NameSize;
            NCGDevice::GetTextExtent(&NameSize, *Item->Data.ItemName[i], 0, 0, 0, 0, 0);
            Device->DrawText(Item->Data.IconW + X + 5, (Item->Data.IconH + 5) * i + NameSize.cy + TextSize.cy + Y + 10,
                             0xFFDCDCDC, CountText, (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
    }
    if (Item->Children.Num() > 0)
    {
        if (Item->bExpanded == 0 && Item->Data.Kind != 0)
        {
            for (INT i = 0; i < Item->Children.Num(); i++)
                Item->Children(i)->HideWindow();
            return Used;
        }
        for (INT i = 0; i < Item->Children.Num(); i++)
        {
            if (Item->Data.Kind == 0)
                Used += DrawItem(Device, Item->Children(i), 3, Used + Y);
            else
                Used += DrawItem(Device, Item->Children(i), Item->Data.IconW / 2 + X, Used + Y);
        }
    }
    return Used;
}
NW_HOOK_METHOD(0x100ea630, NCQuestGaraTree, DrawItem)

NW_CHECK_VTABLE(NCQuestGaraTree, 0x101c5fe0, 79)
