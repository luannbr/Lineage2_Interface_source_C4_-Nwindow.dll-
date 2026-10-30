/*=============================================================================
    NCRecipeBuyListWnd.cpp - the recipe shop as the customer sees it (new in C2).
    Stock (C3): 0x100DEBF0 (WindowProc), 0x100DEC70 (OnClose), 0x100DEC90 / 0x100DECA0 (the owner, the adena),
    0x100DF9B0 (OnPaint), 0x100DFBF0 (OnSelectRecipe), 0x100E0B70 (AddRecipe), 0x100E1F50 .. 0x100E2010 (constructor,
    destructor), 0x100E2010 (OnCreate); OnDestroy is NCTrainWnd's body (0x100C4E30).
=============================================================================*/
#include "nwindow/NCRecipeBuyListWnd.h"
#include "nwindow/NCRecipeListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NClasses.gen.h"

// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12)
enum
{
    STR_RecipeList = 643,    // 0x678B0
    STR_CreatorMP = 644,     // 0x678BC
    STR_Close = 0x286,       // the close button's text
};
enum { SOUND_Close = 6 };    // NConsoleWnd::PlayInterfaceSound

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

// 0x100f31e0 - the list, the textures and the figures cleared; no shop (-1)
NCRecipeBuyListWnd::NCRecipeBuyListWnd()
{
    RecipeList = NULL;
    BackTex = NULL;
    MP = 0;
    MaxMP = 0;
    Adena = 0;
    ShopOwnerID = -1;
}
NW_HOOK_CTOR(0x100f31e0, NCRecipeBuyListWnd)

// 0x100f3250
NW_HOOK_DTOR(0x100f3250, NCRecipeBuyListWnd)

// 0x100efd20 (vslot 25) - the list's click asks for the recipe's ingredients, the close button closes
INT NCRecipeBuyListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_RECIPEBUYLIST_LIST)
            return OnSelectRecipe(lParam) != 0;
        if (wParam == NID_RECIPEBUYLIST_CLOSE)
            return OnClose() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100efd20, NCRecipeBuyListWnd, WindowProc)

// 0x100efda0 (vslot 74) - the window closes with the sound
INT NCRecipeBuyListWnd::OnClose()
{
    HideWindow();
    GConsole->PlayInterfaceSound(SOUND_Close);
    return 1;
}
NW_HOOK_METHOD(0x100efda0, NCRecipeBuyListWnd, OnClose)

// 0x100c11b0 - the list emptied (a body the compiler folded with NCVIPShopWnd's list window: the same code for any window
// whose child at +0x98 is a list; the hook is NCVIPShopItemListWnd::Clear's)
void NCRecipeBuyListWnd::ClearList()
{
    RecipeList->Clear();
}

// 0x100efdd0
void NCRecipeBuyListWnd::SetShopOwner(INT InOwnerID)
{
    ShopOwnerID = InOwnerID;
}
NW_HOOK_METHOD(0x100efdd0, NCRecipeBuyListWnd, SetShopOwner)

// 0x100f1d60 - a recipe row into the list box (a tail call into NCRecipeListBox::AddEntry)
void NCRecipeBuyListWnd::AddRecipe(const NRecipeRow* Src)
{
    RecipeList->AddEntry(Src);
}
NW_HOOK_METHOD(0x100f1d60, NCRecipeBuyListWnd, AddRecipe)

// 0x10084be0 (ret 4) - `mov [ecx + 0x108], eax`, the setter many windows share (NSetField108::Set); the hook build jumps to it, the
// standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED void NCRecipeBuyListWnd::SetAdena(INT InAdena) NW_STOCK(0x10084be0)
#else
void NCRecipeBuyListWnd::SetAdena(INT InAdena)
{
    Adena = InAdena;
}
#endif

// 0x100c4e30 (vslot 38): the stock body is NCTrainWnd's (hooked there): the list and the close button go; the standalone has its
// own copy of that body
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeBuyListWnd::OnDestroy() NW_STOCK(0x1011dc90)// C3: no address for the base's 0x100c4e30
#else
INT NCRecipeBuyListWnd::OnDestroy()
{
    DestroyChild(this, RecipeList);
    DestroyChild(this, CloseButton);
    NCWnd::OnDestroy();
    return 1;
}
#endif

// 0x100f0dc0 (vslot 75) - the clicked recipe: the server is asked what it takes (the shop owner and the recipe's index)
INT NCRecipeBuyListWnd::OnSelectRecipe(LPARAM Index)
{
    NRecipeRow Row;
    if (RecipeList->GetSelectedEntry(&Row))
        GConsole->RequestRecipeShopMakeInfo(ShopOwnerID, Row.RecipeID);
    return 1;
}
NW_HOOK_METHOD(0x100f0dc0, NCRecipeBuyListWnd, OnSelectRecipe)

// 0x100f0b80 (vslot 62; the db's NCRecipeListWnd::OnPaint) - the background, the list's label, "Creator MP" with the MP
// bar (165 pixels at most), the player's adena right-aligned (no icon; the close button is put at its place again)
INT NCRecipeBuyListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeListWnd::OnPaint);   // the stock names it so (the C2 class's name)
    TCHAR AdenaText[132];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 381, 0, 0, 256, 381, BackTex, 0xFF, 1);
    CloseButton->SetPosition(91, 372, 76, 23);
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_RecipeList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(18, 323, 0xFFDCDCDC, *SysString(STR_CreatorMP), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    INT BarW = (INT)((DOUBLE)(MP * 165) / MaxMP);
    if (BarW > 0)
    {
        if (BarW > 165)
            BarW = 165;
        Device->DrawStretchedTex(75, 323, BarW, 12, 0, 0, 8, 12, MPTex, 0xFF);
    }
    NFormatThousands(AdenaText, Adena);
    NCGDevice::GetTextExtent(&Size, AdenaText, 0, 0, 0, 0, 0);
    Device->DrawText(157 - Size.cx, 341, 0xFFDCDCDC, AdenaText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f0b80, NCRecipeBuyListWnd, OnPaint)

// 0x100f32a0 (vslot 37) - the title bar (id 0xFF), the textures, the list (100 rows, 5 shown; the line gap, the text
// and the icon offsets the box takes) and the close button (id 0x1019)
INT NCRecipeBuyListWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_RECIPEBUYLIST_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.RecipeWnd.Recipe3_Back"));
    MPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_mpbar"));

    RecipeList = new NCRecipeListBox(100, 5, 0x10);
    RecipeList->ID = NID_RECIPEBUYLIST_LIST;
    RecipeList->SetPosition(9, 49, 238, 254);
    CreateChild(RecipeList, NWS_VISIBLE);
    RecipeList->TextX = 7;
    RecipeList->TextY = 4;
    RecipeList->LineGap = 4;

    CloseButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    CloseButton->ID = NID_RECIPEBUYLIST_CLOSE;
    CloseButton->SetPosition(91, 372, 76, 23);
    CreateChild(CloseButton, NWS_VISIBLE);
    CloseButton->SetTextID(STR_Close);
    return 1;
}
NW_HOOK_METHOD(0x100f32a0, NCRecipeBuyListWnd, OnCreate)

NW_CHECK_VTABLE(NCRecipeBuyListWnd, 0x101c7460, 76)
