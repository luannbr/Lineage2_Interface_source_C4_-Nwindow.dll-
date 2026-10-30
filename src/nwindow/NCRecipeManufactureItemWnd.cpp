/*=============================================================================
    NCRecipeManufactureItemWnd.cpp - the ingredient grid of the recipe windows (new in C2).
    Stock (C3): 0x100B1E70 (OnMouseMove), 0x100E2D50 (OnPaint), 0x100E4980 (tool tip), 0x100E4FD0 .. 0x100E5070
    (constructors, destructor), 0x100E50A0 / 0x100E5ED0 (the entries).
    WindowProc (0x10090b10), OnCreate (0x100DC9F0) and OnDestroy (0x100D7090) are bodies shared with
    NCSkillItemWnd / NCQuestItemWnd / NCShopItemWnd, hooked there.
=============================================================================*/
#include "nwindow/NCRecipeManufactureItemWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"

/*-----------------------------------------------------------------------------
    NCRecipeManufactureItemWnd
-----------------------------------------------------------------------------*/

// 0x100f6360 - NCItemWnd's constructor and this class's vtable
NCRecipeManufactureItemWnd::NCRecipeManufactureItemWnd()
{
}
NW_HOOK_CTOR(0x100f6360, NCRecipeManufactureItemWnd)

// 0x100f6400 (ret 0x10) - the grid of the four arguments (columns, rows, most items, icon size; the gaps are 1)
NCRecipeManufactureItemWnd::NCRecipeManufactureItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, 1, 1)
{
}
NW_HOOK_CTOR_ARGS(0x100f6400, NCRecipeManufactureItemWnd, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x100f63b0
NW_HOOK_DTOR(0x100f63b0, NCRecipeManufactureItemWnd)

// the stock bodies shared with other item windows (hooked there): NCItemWnd's, with the result as 0 / 1
// 0x100a1050 (vslot 25, ret 0xC): NCSkillItemWnd's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeManufactureItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x100ad790)
#else
INT NCRecipeManufactureItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCItemWnd::WindowProc(Msg, wParam, lParam) != 0;   // 0x100ad790
}
#endif

// 0x100dc9f0 (vslot 37): NCQuestItemWnd's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeManufactureItemWnd::OnCreate() NW_STOCK(0x100edb50)
#else
INT NCRecipeManufactureItemWnd::OnCreate()
{
    NCItemWnd::OnCreate();   // 0x100edb50
    return 1;
}
#endif

// 0x100d7090 (vslot 38): NCShopItemWnd's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeManufactureItemWnd::OnDestroy() NW_STOCK(0x100efac0)// C3: no address for the base's 0x100d7090
#else
INT NCRecipeManufactureItemWnd::OnDestroy()
{
    NCItemWnd::OnDestroy();   // 0x100efac0
    return 1;
}
#endif

// 0x100f41b0 (vslot 62) - NCItemWnd's loop without the selection frame: each icon, then a "ChatBack" square over it when
// the player has less of the ingredient (Count, +0x1B28) than the recipe needs (UnknownC34, +0x1B40). The clip opens a pixel
// around the window before BeginPaint (the size a float plus 2), and the cells are 5 and 3 pixels apart.
INT NCRecipeManufactureItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeManufactureItemWnd::OnPaint);
    Device->SetClip(ScreenX - 1, ScreenY - 1, (INT)(Width + 2.0f), (INT)(Height + 2.0f));
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 0;
    INT Y = 0;
    for (INT i = TopRow * Columns; i < Items.Num(); i++)
    {
        NItemEntry* Item = Items(i);
        if (!Item)
            continue;
        if (i % Columns == 0)
        {
            X = 0;
            if (i != TopRow * Columns)
                Y += CellSize + 3;
            if (VisibleRows * (CellSize + 1) <= Y)
                break;
        }
        else
            X += CellSize + 5;
        if (Item->GetIcon())
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
        if (AsC4(Item)->Count < AsC4(Item)->UnknownC34)
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, FindTexture(TEXT("NWindow.ChatBack")), 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f41b0, NCRecipeManufactureItemWnd, OnPaint)

// 0x100aeb30 (vslot 63; the db's OnPostPaint; the body is shared with NCSkillItemWnd's slot) - the hovered entry's tool
// tip, always the full box (the grid has no selection): the skin gets a copy of the entry's tool tip object by value, the
// cell's position and the window's rectangle
INT NCRecipeManufactureItemWnd::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (HoverIndex >= 0 && HoverIndex < Items.Num())
    {
        INT Index = HoverIndex - TopRow * Columns;
        INT Y = (CellSize + 3) * (Index / Columns);
        INT X = (CellSize + 5) * (Index % Columns);
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
NW_HOOK_METHOD(0x100aeb30, NCRecipeManufactureItemWnd, OnDrawToolTip)

// 0x100bed40 (vslot 67) - the entry under the mouse
INT NCRecipeManufactureItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    HoverIndex = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    return 1;
}
NW_HOOK_METHOD(0x100bed40, NCRecipeManufactureItemWnd, OnMouseMove)

// 0x100f7320 - a copy of Src as a new entry, its tool tip a recipe's (NTT_Recipe); 0 when the grid is full
UBOOL NCRecipeManufactureItemWnd::AddIngredient(NItemEntry* Src)
{
    if (Items.Num() >= MaxItems)
        return 0;
    NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NItemEntry;
    NItemEntryAssign(Item, Src);
    AsC4(Item)->ToolTip.Fill(Item, NTT_Recipe, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100f7320, NCRecipeManufactureItemWnd, AddIngredient)

/*-----------------------------------------------------------------------------
    NItemEntry's plain assignment.
-----------------------------------------------------------------------------*/

// 0x100f6430 - every field: the strings in full (the loops of 0x100, 0x100, 0x80 x 5, 0x800 and 0x100 characters), the
// numbers one by one (the word at +0x1B48 as a word, its padding left alone), the tool tip object last. C4 layout (NItemC4.h):
// five icons first, five icon names, the numbers from +0x1B14, the tool tip at +0x1BC0
NItemEntry* NItemEntryOps::Assign(NItemEntry* Src)
{
    NItemEntryC4* Dst = AsC4(this);
    NItemEntryC4* From = AsC4(Src);
    Dst->Icon[0] = From->Icon[0];
    Dst->Icon[1] = From->Icon[1];
    Dst->Icon[2] = From->Icon[2];
    Dst->Icon[3] = From->Icon[3];
    Dst->Icon[4] = From->Icon[4];
    for (INT i = 0; i < 256; i++)
        Dst->Name[i] = From->Name[i];
    for (INT i = 0; i < 256; i++)
        Dst->AdditionalName[i] = From->AdditionalName[i];
    for (INT i = 0; i < 128; i++)
        Dst->IconName[0][i] = From->IconName[0][i];
    for (INT i = 0; i < 128; i++)
        Dst->IconName[1][i] = From->IconName[1][i];
    for (INT i = 0; i < 128; i++)
        Dst->IconName[2][i] = From->IconName[2][i];
    for (INT i = 0; i < 128; i++)
        Dst->IconName[3][i] = From->IconName[3][i];
    for (INT i = 0; i < 128; i++)
        Dst->IconName[4][i] = From->IconName[4][i];
    for (INT i = 0; i < 2048; i++)
        Dst->Description[i] = From->Description[i];
    for (INT i = 0; i < 256; i++)
        Dst->DisplayName[i] = From->DisplayName[i];
    Dst->Type = From->Type;
    Dst->Unknown1910 = From->Unknown1910;
    Dst->ObjectID = From->ObjectID;
    Dst->ID = From->ID;
    Dst->UnknownC18 = From->UnknownC18;
    Dst->Count = From->Count;
    Dst->UnknownC20 = From->UnknownC20;
    Dst->BodyPart = From->BodyPart;
    Dst->Weight = From->Weight;
    Dst->UnknownC2C = From->UnknownC2C;
    Dst->UnknownC30 = From->UnknownC30;
    Dst->UnknownC34 = From->UnknownC34;
    Dst->Enchant = From->Enchant;
    Dst->UnknownC3C = From->UnknownC3C;
    Dst->bStackable = From->bStackable;
    Dst->bEquipped = From->bEquipped;
    Dst->TradeCount = From->TradeCount;
    Dst->bSelected = From->bSelected;
    Dst->Unknown1C58 = From->Unknown1C58;
    Dst->CrystalGrade = From->CrystalGrade;
    Dst->Unknown1C60 = From->Unknown1C60;
    Dst->Unknown1C64 = From->Unknown1C64;
    Dst->Unknown1C68 = From->Unknown1C68;
    Dst->Unknown1C6C = From->Unknown1C6C;
    Dst->Unknown1C70 = From->Unknown1C70;
    Dst->SubType = From->SubType;
    Dst->Unknown1C78 = From->Unknown1C78;
    Dst->Unknown1C7C = From->Unknown1C7C;
    Dst->Unknown1C80 = From->Unknown1C80;
    Dst->Unknown1C84 = From->Unknown1C84;
    Dst->Unknown1C88 = From->Unknown1C88;
    Dst->Unknown1C8C = From->Unknown1C8C;
    Dst->Unknown1C90 = From->Unknown1C90;
    Dst->Unknown1C94 = From->Unknown1C94;
    Dst->Unknown1C98 = From->Unknown1C98;
    Dst->Unknown1998 = From->Unknown1998;
    Dst->Unknown1C9C = From->Unknown1C9C;
    Dst->Unknown1CA0 = From->Unknown1CA0;
    Dst->Unknown1CA4 = From->Unknown1CA4;
    Dst->Unknown1CA8 = From->Unknown1CA8;
    Dst->Unknown19AC = From->Unknown19AC;
    Dst->Unknown19B4 = From->Unknown19B4;
    Dst->ToolTip = From->ToolTip;
    return this;
}
NW_HOOK_METHOD(0x100f6430, NItemEntryOps, Assign)

NW_CHECK_VTABLE(NCRecipeManufactureItemWnd, 0x101c7db0, 81)
