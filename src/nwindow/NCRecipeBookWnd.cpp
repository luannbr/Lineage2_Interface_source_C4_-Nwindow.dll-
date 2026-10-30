/*=============================================================================
    NCRecipeBookWnd.cpp - the recipe book window (new in C2).
    Stock (C3): 0x100E2410 (OnMouseMove), 0x100E24C0 .. 0x100E253F (Clear, the trash test), 0x100E2B40 .. 0x100E2D0F
    (destroy, paint), 0x100E4530 (AddRecipe), 0x100E4C00 .. 0x100E4D1F (constructor, destructor), 0x100E4D20 (OnCreate),
    0x100E4EC0 (the tool tip).
=============================================================================*/
#include "nwindow/NCRecipeBookWnd.h"
#include "nwindow/NCRecipeBookItemWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"

enum { NID_RECIPEBOOK_FRAME = 0xFF };  // the title bar's id (name ours)
enum { RECIPE_BOOK_MAX = 50 };         // "(n/50)": the most recipes the book holds
// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12)
enum
{
    STR_RecipeBook = 649,              // 0x678F8: the window's title line at (11, 32)
    STR_RecipeBookTrashTip = 890,      // 0x68444: the trash icon's tool tip
};

template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x10119f60 (vslot 25, ret 0xC): the stock body is NControl's (NCWnd's with the result as 0 / 1), not NCWnd's; the standalone has
// its own copy of that body
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeBookWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT NCRecipeBookWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// 0x100f5f70 - the tool tip is constructed, the trash icon's rectangle is (208, 347) 34 x 34
NCRecipeBookWnd::NCRecipeBookWnd()
{
    TrashY = 0;
    TrashX = 0;
    TrashW = 0;
    TrashH = 0;
    Unknown0BC = 0;
    TrashW = 34.0f;
    TrashH = 34.0f;
    TrashX = 0xD0;
    TrashY = 0x15B;
}
NW_HOOK_CTOR(0x100f5f70, NCRecipeBookWnd)

// 0x100f6030
NW_HOOK_DTOR(0x100f6030, NCRecipeBookWnd)

// 0x100f3770 - the grid emptied
void NCRecipeBookWnd::Clear()
{
    Items->Clear();
    Unknown0BC = 0;
}
NW_HOOK_METHOD(0x100f3770, NCRecipeBookWnd, Clear)

// 0x100f3790 (ret 8; sub_ in the db) - (X, Y) inside the trash icon's rectangle (its size in floats, cut)
UBOOL NCRecipeBookWnd::HitTrash(INT X, INT Y)
{
    INT RelX = X - (TrashX + ScreenX);
    INT RelY = Y - (TrashY + ScreenY);
    if (RelX >= 0 && RelX <= (INT)TrashW && RelY >= 0 && RelY <= (INT)TrashH)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x100f3790, NCRecipeBookWnd, HitTrash)

// 0x100f59a0 - into the grid, always 1
INT NCRecipeBookWnd::AddRecipe(NItemInfo* Info)
{
    if (Items)
        Items->AddItem(Info);
    return 1;
}
NW_HOOK_METHOD(0x100f59a0, NCRecipeBookWnd, AddRecipe)

// 0x100f36c0 (vslot 67) - the mouse over the trash icon (its rectangle in floats: the point is compared as one)
INT NCRecipeBookWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = (INT)(lParam & 0xFFFF);
    INT Y = (INT)((DWORD)lParam >> 16);
    INT Left = TrashX + ScreenX;
    if (Left <= X && (FLOAT)X <= (FLOAT)Left + TrashW)
    {
        INT Top = TrashY + ScreenY;
        if (Top <= Y && (FLOAT)Y <= (FLOAT)Top + TrashH)
        {
            bOverTrash = 1;
            return 1;
        }
    }
    bOverTrash = 0;
    return 1;
}
NW_HOOK_METHOD(0x100f36c0, NCRecipeBookWnd, OnMouseMove)

// 0x100f3f90 (vslot 38)
INT NCRecipeBookWnd::OnDestroy()
{
    DestroyChild(this, Items);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100f3f90, NCRecipeBookWnd, OnDestroy)

// 0x100f3fd0 (vslot 62) - the background, the trash icon (lit while a recipe is dragged over it), the title and the
// count of recipes "(n/limit)" right-aligned in orange; the limit is the one of the book's kind (C3 printed a fixed 50)
INT NCRecipeBookWnd::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeBookWnd::OnPaint);
    TCHAR Count[32];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 381, 0, 0, 256, 381, BackTex, 0xFF, 1);
    Device->DrawTexture(TrashX, TrashY, 32, 32, 0, 0, 32, 32, bTrashActive ? TrashActiveTex : TrashTex, 0xFF, 1);
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_RecipeBook), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (Items)
    {
        appSprintf(Count, TEXT("(%d/%d)"), Items->Items.Num(), Kind == 1 ? LimitKind1 : LimitOther);
        NCGDevice::GetTextExtent(&Size, Count, 0, 0, 0, 0, 0);
        Device->DrawText((INT)(Width - Size.cx - 10.0f), 33, 0xFFB09B79, Count, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f3fd0, NCRecipeBookWnd, OnPaint)

// 0x100f6240 (vslot 57; the db's OnPostPaint) - the trash icon's tool tip, drawn by the window's skin at the icon's corner
INT NCRecipeBookWnd::OnDrawToolTip(NCGDevice* Device)
{
    guard(NCRecipeBookWnd::OnPostPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bOverTrash != 0 && ToolTipSkin != NULL)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, TrashToolTip, TrashX, TrashY, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f6240, NCRecipeBookWnd, OnDrawToolTip)

// 0x100f6090 (vslot 37) - the title bar (id 0xFF), the textures, the grid (6 columns, 8 rows, 160 recipes, 32 pixel
// cells with gaps 5 and 3) and the trash icon's tool tip
INT NCRecipeBookWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_RECIPEBOOK_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.RecipeWnd.Recipe2_Back"));
    TrashTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.Inventory_trash"));
    TrashActiveTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.Inventory_trash_Drag"));
    Items = new NCRecipeBookItemWnd(6, 8, 0xA0, 0x20, 5, 3);
    Items->SetPosition(9, 48, 239, 279);
    CreateChild(Items, NWS_VISIBLE | NWS_UNKNOWN20);
    bOverTrash = 0;
    // NToolTipInfo::SetText of this tree is a trap (its C3 address); the stock one is 0x10032460 (ret 8): Clear, the text, the
    // style, Layout
    TrashToolTip.SetText((const TCHAR*)*SysString(STR_RecipeBookTrashTip), 0x400);   // 0x10032460
    return 1;
}
NW_HOOK_METHOD(0x100f6090, NCRecipeBookWnd, OnCreate)

NW_CHECK_VTABLE(NCRecipeBookWnd, 0x101c7c08, 74)
