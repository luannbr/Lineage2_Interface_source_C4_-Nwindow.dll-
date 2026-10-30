/*=============================================================================
    NCQuestGaraTreeWnd.cpp - the quest tab of the main window. C3 stock: 0x10084280 (destructor, among the GM quest
    window's code), 0x100DA270 .. 0x100DA436 (WindowProc, ClearList, ShowWindow, SetToolTipInfo), 0x100DB400 ..
    0x100DB73E (OnDestroy, OnMouseMove), 0x100DBB50 .. 0x100DBEA4 (paint), 0x100DC280 .. 0x100DC956 (constructor,
    OnCreate, tool tip). Slots 69 / 70 (0x100DB4A0, 0x100DC050) are bodies the db names NCGMQuestWnd's: they are
    this class' and the GM's window inherits them; 0x100DB5D0 (the mark off) is the small helper both call.
=============================================================================*/
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NWinBStock.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_QUESTTREE_CANCEL = 0xFD,
    NID_QUESTTREE_MARK = 0xFF00FF23,
};

// sysstring lines (names ours)
enum
{
    STR_AcceptedQuest = 324,
    STR_QuestInfo1 = 861,            // the five lines of the "?" icon's tool tip
    STR_QuestInfo5 = 865,
    STR_GiveUp = 0x181,              // the button's text
    STR_MarkOnMap = 0x35C,           // the check box's label
};

enum { MAX_QUESTS = 15 };            // the "(n/15)" of the title

// the delete-quest dialogs (names ours)
enum
{
    MSG_QuestDelete = 0xB6,          // the confirmation (named by the stock code's argument)
    MSG_QuestNone = 0x4B1,           // nothing chosen
    DIALOG_QuestDelete = 0x1E,       // the box's ids
    DIALOG_QuestNone = 0x1F,
};

// how many characters the FString holds, as the stock tests it (the array's count, not the length)
static FORCEINLINE INT StringNum(const FString& S)
{
    return ((const TArray<TCHAR>*)&S)->Num();
}

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100ed2a0 - NCWnd's constructor; the tree, the cancel button, the two flags, the count and the tool tip flag empty (the
// FString and the tool tip record are made by their own constructors)
NCQuestGaraTreeWnd::NCQuestGaraTreeWnd()
{
    TreeCtrl = NULL;
    CancelButton = NULL;
    NumQuests = 0;
    bDC = 0;
    bDD = 0;
    bShowToolTip = 0;
}
NW_HOOK_CTOR(0x100ed2a0, NCQuestGaraTreeWnd)

// 0x1008f590 - the tool tip and the text go, then NCWnd's
NW_HOOK_DTOR(0x1008f590, NCQuestGaraTreeWnd)

// 0x100ea070 (vslot 25) - the cancel button and the check box
INT NCQuestGaraTreeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_QUESTTREE_CANCEL)
            return OnCancelQuest() != 0;
        if (wParam == NID_QUESTTREE_MARK)
            return OnMarkCheckBox() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100ea070, NCQuestGaraTreeWnd, WindowProc)

// 0x100eb6f0 (vslot 69; the db names it NCGMQuestWnd::OnDialogResult, the GM window inherits it) - the quest the tree
// marked (0x100DB1A0): with one, the confirmation box 0x1E (system message 182) carrying its id; with none, the notice
// 0x1F (system message 1201)
INT NCQuestGaraTreeWnd::OnCancelQuest()
{
    // the stock frame holds a dialog record per branch (0x140 bytes); the compiler would share one slot, and the frame
    // audit (check_frames) takes the smaller frame for a grown struct: the records are made in a buffer of both
    BYTE Records[2 * sizeof(NDialogInfo)];
    INT Quest = TreeCtrl->FindMarkedQuest();
    if (Quest > 0)
    {
        NDialogInfo* Info = ::new (Records) NDialogInfo;
        Info->ID = DIALOG_QuestDelete;
        Info->Owner = Entry;
        Info->bMainBox = 1;
        Info->bInput = 0;
        Info->SetSystemMessage(MSG_QuestDelete);
        Info->SetParams(Quest, -1);
        GConsole->ShowDialog(Info);
        Info->~NDialogInfo();
    }
    else
    {
        NDialogInfo* Info = ::new (Records + sizeof(NDialogInfo)) NDialogInfo;
        Info->ID = DIALOG_QuestNone;
        Info->bMainBox = 1;
        Info->bInput = 0;
        Info->SetSystemMessage(MSG_QuestNone);
        GConsole->ShowDialog(Info);
        Info->~NDialogInfo();
    }
    return 1;
}
NW_HOOK_METHOD(0x100eb6f0, NCQuestGaraTreeWnd, OnCancelQuest)

// 0x100ec5e0 (vslot 70; the db names it NCGMQuestWnd::OnDestroy) - with the check box off (+0x1F0 of the control at
// +0xD4) the map's mark and the two flags are cleared
INT NCQuestGaraTreeWnd::OnMarkCheckBox()
{
    if (!MarkCheckBox->bChecked)
        ClearMark();
    return 1;
}
NW_HOOK_METHOD(0x100ec5e0, NCQuestGaraTreeWnd, OnMarkCheckBox)

// 0x100ea170 - the fourth number SetToolTipInfo was given
INT NCQuestGaraTreeWnd::GetTrackedQuestID()
{
    return ToolTipInfo[3];
}
NW_HOOK_METHOD(0x100ea170, NCQuestGaraTreeWnd, GetTrackedQuestID)

// 0x100ec100 - the map's mark off (the desktop's NCMapCoverWnd, its map's bShowMark) and the two flags cleared. The stock
// tests the three links one after the other (C4: GConsole +0x3BF4 the desktop, +0x170 the map cover, +0xE8 its map,
// +0x1B4 the flag)
void NCQuestGaraTreeWnd::ClearMark()
{
    NCConsole* Desktop = GConsole->GetDesktop();
    if (Desktop)
    {
        NCMapCoverWnd* Cover = (NCMapCoverWnd*)Desktop->MapCoverWnd;
        if (Cover)
        {
            if (Cover->Map)
                Cover->Map->bShowMark = 0;
        }
    }
    bDC = 0;
    bDD = 0;
}
// 0x100ec100 is already hooked in NConsoleWndAnon.cpp (NCFlagWndExt::ClearFlags, the same body through raw offsets): one
// address, one hook, so this class' real method is only the body OnMarkCheckBox calls
// NW_HOOK_METHOD(0x100ec100, NCQuestGaraTreeWnd, ClearMark)

// 0x100ea0f0 (vslot 68, name ours) - the tree emptied and its scroll back, the count 0
void NCQuestGaraTreeWnd::ClearList()
{
    TreeCtrl->ResetScroll();
    NumQuests = 0;
}
NW_HOOK_METHOD(0x100ea0f0, NCQuestGaraTreeWnd, ClearList)

// 0x100ea110 (vslot 48) - emptied, then filled again by the server's answer
void NCQuestGaraTreeWnd::ShowWindow()
{
    ClearList();
    ::nw::VirtualCall<UBOOL>(GConsole, 432);   // RequestQuestList: by number (as NCQuestWnd::ShowWindow does)
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x100ea110, NCQuestGaraTreeWnd, ShowWindow)

// 0x100ea1d0 (ret 0x1C) - the text and four numbers of the selected quest's map place; the by-value text is destroyed here,
// as the stock does after copying it
void NCQuestGaraTreeWnd::SetToolTipInfo(FString Text, INT A, INT B, INT C, INT D)
{
    ToolTipText = Text;
    ToolTipInfo[0] = A;
    ToolTipInfo[1] = B;
    ToolTipInfo[2] = C;
    ToolTipInfo[3] = D;
}
NW_HOOK_METHOD(0x100ea1d0, NCQuestGaraTreeWnd, SetToolTipInfo)

// 0x100eb650 (vslot 38) - no guard here
INT NCQuestGaraTreeWnd::OnDestroy()
{
    DestroyChild(this, TreeCtrl);
    DestroyChild(this, CancelButton);
    DestroyChild(this, MarkCheckBox);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100eb650, NCQuestGaraTreeWnd, OnDestroy)

// 0x100eb8e0 (vslot 67) - the "?" icon (15 x 15 at (232, 10)) shows the tool tip while the mouse is over it
INT NCQuestGaraTreeWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    NRectF Icon;
    Icon.X = ScreenX + 0xE8;
    Icon.Y = ScreenY + 0xA;
    Icon.W = 15.0f;
    Icon.H = 15.0f;
    bShowToolTip = PtInRect((SHORT)lParam, (SHORT)(lParam >> 16), Icon) != 0;
    return 1;
}
NW_HOOK_METHOD(0x100eb8e0, NCQuestGaraTreeWnd, OnMouseMove)

// 0x100ed8e0 (vslot 63) - the icon's tool tip, drawn by the window's skin
INT NCQuestGaraTreeWnd::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bShowToolTip && ToolTipSkin)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, ToolTip, 0xE8, 0xA, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100ed8e0, NCQuestGaraTreeWnd, OnDrawToolTip)

// 0x100ec230 (vslot 62) - the panel, the title with the count and the "?" icon; then the selected quest: the counts
// of the items it needs (what the inventory's two pages hold) are updated in its last child's record and the
// quest's map place is marked while the check box is on. The two flags (bDC, bDD) tell the console there is none.
// C4: showing the mark also empties the map's own list of marks (MarkPos)
INT NCQuestGaraTreeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCQuestGaraTreeWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 256, 335, 0, 0, 256, 335, BackTex, 0xFF, 1);
    Device->DrawText(11, 12, 0xFFDCDCDC, *GL2GameData.SysString[STR_AcceptedQuest], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    TCHAR CountText[16];
    appSprintf(CountText, TEXT("(%d/%d)"), NumQuests, MAX_QUESTS);
    Device->DrawText(190, 12, 0xFFB09B79, CountText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawTexture(232, 10, 15, 15, 0, 0, 15, 15, ToolTipBtnTex, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();

    NCGaraTVItem* Selected = TreeCtrl->QuestTree->SelectedItem;
    if (Selected)
    {
        NCGaraTVItem* Last = NULL;
        if (Selected->Children.Num() > 0)
            Last = Selected->Children(Selected->Children.Num() - 1);
        if (Last)
        {
            for (INT i = 0; i < Last->Data.Count; i++)
            {
                Last->Data.ItemOwned[i] = 0;
                NCInvenItemWnd* QuestPage = GDesktop->InventoryWnd->ItemWnd[1];
                if (QuestPage)
                {
                    for (INT j = 0; j < QuestPage->Items.Num(); j++)
                    {
                        NItemEntry* Entry = QuestPage->Items(j);
                        if (Last->Data.ItemID[i] == EntryID(Entry))
                            Last->Data.ItemOwned[i] += EntryCount(Entry);
                    }
                }
                NCInvenItemWnd* ItemPage = GDesktop->InventoryWnd->ItemWnd[0];
                if (ItemPage)
                {
                    for (INT j = 0; j < ItemPage->Items.Num(); j++)
                    {
                        NItemEntry* Entry = ItemPage->Items(j);
                        if (Last->Data.ItemID[i] == EntryID(Entry))
                            Last->Data.ItemOwned[i] += EntryCount(Entry);
                    }
                }
            }
        }
        if (Selected->bExpanded)
            TreeCtrl->SelectedID = Selected->Data.QuestID;
        else
            TreeCtrl->SelectedID = 0;
        if (MarkCheckBox->bChecked && Selected->Data.bTextCount == 0 && StringNum(Selected->Data.MapPlace) != 0
            && StringNum(Selected->Data.MapPlace) - 1 > 0)
        {
            ((NCQuestGaraTreeWnd*)GDesktop->MainWnd->QuestWnd)->SetToolTipInfo(
                Selected->Data.MapPlace, Selected->Data.MapPos[0], Selected->Data.MapPos[1],
                Selected->Data.MapPos[2], Selected->Data.QuestID);
            if (GDesktop)
            {
                NCMapCoverWnd* Cover = (NCMapCoverWnd*)GDesktop->MapCoverWnd;
                if (Cover && Cover->Map)
                {
                    Cover->Map->bShowMark = 1;
                    Cover->Map->MarkPos.Empty();
                }
            }
            if (Selected->Data.Unknown1DD4 == 0)
            {
                bDC = 1;
                bDD = 1;
            }
        }
        else
        {
            ClearMark();   // the map's mark off and the two flags cleared
        }
    }
    else
    {
        ClearMark();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ec230, NCQuestGaraTreeWnd, OnPaint)

// 0x100ed330 (vslot 37) - the panel, the quest control, the "give up" button, the check box and the tool tip of the
// "?" icon (five lines, each with its own icon). C4: the same flow as C3 (checked against the C4 listing: the positions,
// the ids and the sysstring numbers are the same); the check box is made by the four-argument constructor, the tool
// tip's list setter is 0x10030670 and the two arrays' destructors are 0x1008E8A0 (the strings') and 0x1001F6E0 (the
// textures')
INT NCQuestGaraTreeWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndBack"));
    ToolTipBtnTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndToolTipBtn"));

    TreeCtrl = new NCQuestGaraTreeCtrl();
    TreeCtrl->SetPosition(7, 27, 242, 274);
    CreateChild(TreeCtrl, NWS_VISIBLE);

    CancelButton = new NCButton(TEXT("L2UI_CH3.BUTTON.Btn1_normal"), TEXT("L2UI_CH3.BUTTON.Btn1_normalOn"), 0);
    CancelButton->ID = NID_QUESTTREE_CANCEL;
    CancelButton->SetPosition(91, 306, 77, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_GiveUp);

    MarkCheckBox = new NCCheckBox(STR_MarkOnMap, 0, 0, 0);
    MarkCheckBox->ID = NID_QUESTTREE_MARK;
    MarkCheckBox->SetPosition(10, 310, 100, 12);
    CreateChild(MarkCheckBox, NWS_VISIBLE);
    MarkCheckBox->bChecked = 1;

    // the two arrays' destructors are the stock out-of-line ones, called by address so the inline template code is not
    // repeated here
    BYTE NamesStorage[sizeof(TArray<FString*>)];
    BYTE IconsStorage[sizeof(TArray<UTexture*>)];
    TArray<FString*>& Names = *new ((void*)NamesStorage) TArray<FString*>();
    TArray<UTexture*>& Icons = *new ((void*)IconsStorage) TArray<UTexture*>();
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo1]));
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo1 + 1]));
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo1 + 2]));
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo1 + 3]));
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo5]));
    Icons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_1")));
    Icons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_2")));
    Icons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_3")));
    Icons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_4")));
    Icons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_5")));
    // NToolTipInfo's list setter (0x10030670, ret 0xC; ported in NCGMWnd.cpp): the lines with their icons, style 0x1000
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x10030670, &ToolTip, &Names, &Icons, 0x1000);
#else
    ((NToolTipInfoLinesView*)&ToolTip)->SetIconLines(&Names, &Icons, 0x1000);
#endif
    for (INT i = 0; i < Names.Num(); i++)
    {
        Names(i)->Empty();
        delete Names(i);
    }
    Names.Empty();
    Icons.Empty();
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x1008E8A0, &Names);
    ::nw::StockCallAt<void>(0x1001F6E0, &Icons);
#else
    Names.~TArray<FString*>();   // the stock's out-of-line destructors 0x1008E8A0 / 0x1001F6E0
    Icons.~TArray<UTexture*>();
#endif
    return 1;
}
NW_HOOK_METHOD(0x100ed330, NCQuestGaraTreeWnd, OnCreate)

NW_CHECK_VTABLE(NCQuestGaraTreeWnd, 0x101b47f8, 77)
