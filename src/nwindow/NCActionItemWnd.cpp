/*=============================================================================
    NCActionItemWnd.cpp - a grid of action icons. Stock: 0x10033990 .. 0x10033d95 (but for
    the accessors the stock code shares with other structs), 0x100343e0 .. 0x10034d09,
    0x100368b0 .. 0x10036c64, 0x10036f00 .. 0x1003705f.
=============================================================================*/
#include "nwindow/NCActionItemWnd.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCMacroEditWnd.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NClasses.gen.h"

#if NW_FEATURES
// phase 6: ManufactureCmd (NWindow_C1 src/inject/ManufactureCmd.cpp)
#include "features/ManufactureCmd.h"
#endif

enum { SOUND_PickUp = 3 };   // the interface sound of picking an icon up (name ours)

// the "/" a macro line made from an action starts with: this unit's pointer to the string at 0x10107e78 (name ours)
#if NW_HOOK_MODE
#define GActionItemCmdPrefix NW_GLOBAL(const TCHAR*, 0x1023AA84)
#else
extern const TCHAR* GActionItemCmdPrefix;
#endif

// the chat window
static NCChatWnd* ChatWnd()
{
    return GDesktop->ChatWnd;
}

/*-----------------------------------------------------------------------------
    NActionItem.
-----------------------------------------------------------------------------*/

// 0x1004ff60 (ret 4) - an entry built by NItemEntry's default constructor (0x1001F7F0), the action's id and parameter
// cleared to -1 and the icon, the names and the bit of the selection cleared as the stock does (stores, in this order), then
// the numbers and the three texts the action window uses copied from Info: the action id (+0x1920), the parameter
// (+0x1914), the icon (+0x1900), the name (+0x000), the icon's name (+0x400) and the description (+0x900)
NActionItem::NActionItem(const NItemInfo& Info)
{
    ActionID = -1;
    Param = -1;
    Icon = NULL;
    Name[0] = 0;
    Description[0] = 0;
    bSelected = 0;
    ActionID = Info.ID;
    Param = Info.Param;
    Icon = Info.Icon;
    appStrcpy(Name, Info.Name);
    appStrcpy(IconName, Info.IconName);
    appStrcpy(Description, Info.Description);
}
NW_HOOK_CTOR_ARGS(0x1004ff60, NActionItem, (const NItemInfo& A), (A))

// 0x1004c360 - the action's id (at +0x1C40 in C4)
INT NActionItem::GetActionID()
{
    return ActionID;
}
NW_HOOK_METHOD(0x1004c360, NActionItem, GetActionID)

/*-----------------------------------------------------------------------------
    NCActionItemWnd.
-----------------------------------------------------------------------------*/

// 0x10050350 - an empty grid; the icons are framed with the inventory's textures (the two of the pressed and the idle cell).
// The page's InternalConstructor passes (5, 2, 15, 32)
NCActionItemWnd::NCActionItemWnd(INT InCols, INT InRows, INT InMaxItems, INT InIconSize)
{
    ::nw::Field<INT>(this, 0xE8) = 0;   // Items: the three words cleared (the stock constructor stores them one by one)
    ::nw::Field<INT>(this, 0xEC) = 0;
    ::nw::Field<INT>(this, 0xF0) = 0;
    Cols = InCols;
    Rows = InRows;
    MaxItems = InMaxItems;
    PressedIndex = -1;
    HoverIndex = -1;
    Unknown10C = 0;
    Unknown110 = 0;
    IconSize = InIconSize;
    bDragging = 0;
    bPressed = 0;
    PressedTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine_down"), 1);
    FrameTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine"), 1);
}
NW_HOOK_CTOR_ARGS(0x10050350, NCActionItemWnd, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x10050450 - Items' destructor is TArray<NActionItem*>'s (0x1004a440)
NW_HOOK_DTOR(0x10050450, NCActionItemWnd)


// stock body shared with NControl's (0x10004e90)
INT NCActionItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100ef780 - NCWnd's OnCreate and 1: the one body several classes share, hooked by NCRadarWnd (not ours to hook twice)
INT NCActionItemWnd::OnCreate()
{
    NCWnd::OnCreate();
    return 1;
}

// 0x10050bb0 (vslot 38) - the entries go (each one's tool tip object, then the block), then the array and the window
INT NCActionItemWnd::OnDestroy()
{
    guard(NCActionItemWnd::OnDestroy);
    for (INT i = 0; i < Items.Num(); i++)
    {
        NActionItem* Item = Items(i);
        if (Item)
        {
            EntryToolTip(Item)->~NToolTipInfo();   // inline in the stock code
            appFree(Item);
        }
    }
    Items.Empty();
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10050bb0, NCActionItemWnd, OnDestroy)

// 0x1004cf20 (vslot 62) - the icons IconSize + 5 apart across and IconSize + 3 down, each in its frame (34 x 34, one
// pixel out); the pressed one covered with the pressed frame. The clip is set after the origin; C3 draws nothing for an
// icon without a texture (and keeps the position of the cell: the position moves for every non-NULL entry)
INT NCActionItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCActionItemWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 1;
    INT Y = 1;
    for (INT i = 0; i < Items.Num(); i++)
    {
        NActionItem* Item = Items(i);
        if (!Item)
            continue;
        if (i % Cols == 0)
        {
            X = 1;
            if (i)
                Y += IconSize + 3;
        }
        else
            X += IconSize + 5;
        if (Item->GetIcon())
        {
            Device->DrawTexture(X, Y, IconSize, IconSize, 0, 0, IconSize, IconSize, Item->GetIcon(), 0xFF, 1);
            Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, FrameTex, 0xFF, 1);
            if (PressedIndex == i && bPressed)
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, PressedTex, 0xFF, 1);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1004cf20, NCActionItemWnd, OnPaint)

// 0x100504b0 (vslot 63) - the tool tip of the icon under the mouse: the skin draws the icon's tool tip object (by value)
// at the cell's position (IconSize + 5 across, IconSize + 3 down) and the window's rectangle moves the box to keep it on the
// screen
INT NCActionItemWnd::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (HoverIndex >= 0 && HoverIndex < Items.Num())
    {
        INT Y = (IconSize + 3) * (HoverIndex / Cols);
        INT X = (IconSize + 5) * (HoverIndex % Cols);
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            DrawEntryToolTip(ToolTipSkin, Device, Items(HoverIndex), X, Y, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100504b0, NCActionItemWnd, OnDrawToolTip)

// 0x1004d0f0 (vslot 67) - the icon under the mouse; while an icon is carried the cursor is its icon and an edit line of
// the macro editor (the window the desktop keeps at +0x154) under the mouse is marked (+0x228 of the edit box); anywhere
// else the editor is told to unmark (0x100AA520). (NCMacroEditList::HitLine, 0x100A9E20, finds the line under a point.)
INT NCActionItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    HoverIndex = HitTest(X, Y);
    if (IsFocused() && bDragging)
    {
        if (PressedIndex >= 0 && PressedIndex < Items.Num())
        {
            SetCursor(4, Items(PressedIndex)->GetIcon());
            LockCursor();
        }
        NWndEntry* Found = FindWindowAt(X, Y);
        if (Found && Found->Wnd && Found->Wnd->IsA(NCEditBox::StaticClass()))
        {
            // the edit line's grandparent: the macro editor
            NCWnd* Editor = NULL;
            if (Found->Parent && Found->Parent->Parent)
                Editor = Found->Parent->Parent->Wnd;
            if (Editor)
            {
                NCConsole* Desk = GDesktop;
                if (Desk && Desk->NewWndFC == Editor)
                {
                    NCMacroEditList* Lines = ((NCMacroEditWnd*)Editor)->EditList;
                    INT Line = Lines->HitLine(X, Y);
                    if (Line < 0)
                        return 1;
                    Lines->Edits(Line)->Unknown1D8 = 1;
                    return 1;
                }
                NCWnd* Target = Editor;
                if (Desk)
                    Target = Desk->NewWndFC;
                if (!Target)
                    return 1;
                ((NCMacroEditWnd*)Target)->ClearMarks();
                return 1;
            }
        }
        if (GConsole && GDesktop && GDesktop->NewWndFC)
        {
            ((NCMacroEditWnd*)GDesktop->NewWndFC)->ClearMarks();
            return 1;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1004d0f0, NCActionItemWnd, OnMouseMove)

// 0x1004eb90 (vslot 68) - the icon under the mouse is picked: the others are unselected, it is selected and carried
INT NCActionItemWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT Index = HitTest(LOWORD(lParam), HIWORD(lParam));
    PressedIndex = Index;
    HoverIndex = Index;
    if (Index >= 0 && Index < Items.Num())
    {
        ClearSelection();
        Items(Index)->Select();
        bDragging = 1;
        bPressed = 1;
        GConsole->PlayInterfaceSound(SOUND_PickUp);
    }
    return 1;
}
NW_HOOK_METHOD(0x1004eb90, NCActionItemWnd, OnLButtonDown)

// 0x1004ec10 (vslot 69) - released on the icon pressed: the action runs; dropped on a slot of the shortcut bar: the
// server registers it there (type 3, the slot, the action id and its kind, 1 for the first fifteen, 2 for the pet and
// summon ones up to 27, none after); dropped on a line of the macro editor (the window the desktop keeps at +0x154)
// the action's command ("/<command>", from the game data) is written there when it fits in 32 characters, and the
// actions a macro cannot do (the ids 0x14, 0x1B, 0x20, 0x24, 0x27, 0x29..0x31 and those above 999) tell so in the chat
// instead (systemmsg 1257); the editor is told the line (0x100AA5D0)
INT NCActionItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    bPressed = 0;
    INT Pressed = PressedIndex;
    if (Pressed >= 0 && Pressed < Items.Num())
    {
        INT X = LOWORD(lParam);
        INT Y = HIWORD(lParam);
        if (HitTest(X, Y) == Pressed)
            UseAction();
        else if (bDragging && Items(Pressed) != NULL)
        {
            NWndEntry* Found = FindWindowAt(X, Y);
            if (Found && Found->Wnd)
            {
                NCWnd* Wnd = Found->Wnd;
                if (Wnd->IsA(NCShortCutWnd::StaticClass()))
                {
                    // the bar's slot under the point (by the bar's layout), -1 none
                    INT Slot = ((NCShortCutWnd*)Wnd)->SlotAtPoint(X, Y);
                    if (Slot >= 0)
                    {
                        L2ParamStack Params(10);
                        Params.PushBack((void*)3);  // an action
                        Params.PushBack((void*)(INT_PTR)Slot);
                        Params.PushBack((void*)(INT_PTR)Items(Pressed)->GetActionID());
                        if (Items(Pressed)->GetActionID() >= 0 && Items(Pressed)->GetActionID() <= 14)
                            Params.PushBack((void*)1);
                        else if (Items(Pressed)->GetActionID() > 14 && Items(Pressed)->GetActionID() < 28)
                            Params.PushBack((void*)2);
                        GConsole->RequestShortCutReg(Params);
                    }
                }
                else if (Wnd->IsA(NCEditBox::StaticClass()))
                {
                    // the edit line's grandparent: the macro editor
                    NCWnd* Editor = NULL;
                    if (Found->Parent && Found->Parent->Parent)
                        Editor = Found->Parent->Parent->Wnd;
                    INT Line = -1;
                    if (Editor && GDesktop && GDesktop->NewWndFC == Editor)
                        Line = ((NCMacroEditWnd*)Editor)->EditList->HitLine(X, Y);
                    if (Line >= 0)
                    {
                        INT ActionID = Items(Pressed)->GetActionID();
                        if (ActionID == 0x14 || ActionID == 0x1B || ActionID == 0x20 || ActionID == 0x24 ||
                            ActionID == 0x27 || (ActionID >= 0x29 && ActionID <= 0x31) || ActionID >= 1000)
                        {
                            // a macro cannot do it (systemmsg 1257)
                            GDesktop->ChatWnd->AddChatMessage(*GL2GameData.SystemMsg[1257].Message, 0xFFB09B79, 5, 0);
                        }
                        else
                        {
                            TCHAR Command[1024];
                            if (ActionID < 0)
                                appMemset(Command, 0, 0x400);   // 0x400 bytes of the 2 KB buffer, as in the stock code
                            else
                                appSprintf(Command, TEXT("%s%s"), GActionItemCmdPrefix,
                                           GL2GameData.GetCmdStringByActionID(ActionID));
                            if (appStrlen(Command) <= 32)
                            {
                                NCEditBox* Edit = ((NCMacroEditWnd*)Editor)->EditList->Edits(Line);
                                Edit->ClearText();
                                if (Command[0])
                                    Edit->InsertText(Command, 1);
                            }
                        }
                        ((NCMacroEditWnd*)Editor)->ClearMark(Line);
                    }
                }
            }
        }
        ClearSelection();
        PressedIndex = -1;
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x1004ec10, NCActionItemWnd, OnLButtonUp)

// 0x1004d240 (vslot 74) - the pressed icon's action runs (C2: the console's UseAction takes two more arguments, 0)
void NCActionItemWnd::UseAction()
{
    ::nw::TraceCall("NCActionItemWnd::UseAction", this, PressedIndex);
    NActionItem* Item = Items(PressedIndex);
    if (Item)
        GConsole->UseAction(Item->GetActionID(), 0, 0);   // 0x1006b0b0
}
NW_HOOK_METHOD(0x1004d240, NCActionItemWnd, UseAction)

// 0x1004c370 - the icon under a screen point (cells IconSize + 5 wide and IconSize + 3 high), -1 none
INT NCActionItemWnd::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0 || RX > Cols * (IconSize + 5) || RY < 0 || RY > Rows * (IconSize + 3))
        return -1;
    INT Col = RX / (IconSize + 5);
    if (Col >= Cols)
        Col--;
    INT Row = RY / (IconSize + 3);
    if (Row >= Rows)
        Row--;
    return Cols * Row + Col;
}
NW_HOOK_METHOD(0x1004c370, NCActionItemWnd, HitTest)

// 0x1004d270
void NCActionItemWnd::ClearSelection()
{
    for (INT i = 0; i < Items.Num(); i++)
        Items(i)->Unselect();
}
NW_HOOK_METHOD(0x1004d270, NCActionItemWnd, ClearSelection)

// 0x10050040 (ret 4) - a new entry for Info (none when the grid is full), its tool tip filled with the names only (flags 8)
INT NCActionItemWnd::AddItem(NItemInfo* Info)
{
    if (MaxItems <= Items.Num())
        return 0;
    NActionItem* Item = (NActionItem*)appMalloc(sizeof(NActionItem), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NActionItem(*Info);
    EntryToolTip(Item)->Fill(Item, 8, 0);   // NToolTipInfo::Fill, on the entry's tool tip
    Items.AddItem(Item);
    return 1;
}
NW_HOOK_METHOD(0x10050040, NCActionItemWnd, AddItem)

// 0x100505a0 - the entries deleted (each one's tool tip, 0x10050030, and the block) and the array emptied
void NCActionItemWnd::ClearItems()
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (Items(i))
        {
            static_cast<NItemEntry*>(Items(i))->~NItemEntry();
            appFree(Items(i));
        }
    }
    Items.Empty();
}
NW_HOOK_METHOD(0x100505a0, NCActionItemWnd, ClearItems)

NW_CHECK_VTABLE(NCActionItemWnd, 0x101aaf38, 75)
