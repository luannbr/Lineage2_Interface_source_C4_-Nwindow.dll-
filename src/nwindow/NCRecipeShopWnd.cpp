/*=============================================================================
    NCRecipeShopWnd.cpp - the dwarven recipe shop's manage window (new in C2) and its title bar.
    Stock (C3): 0x100DE920 (the title bar's close), 0x100DE940 .. 0x100DF3B3 (WindowProc, the cancel / quit handlers,
    paint, destroy, start, add product, message), 0x100E0920 .. 0x100E0960 (the grids' forwarders), 0x100E1640 /
    0x100E1690 (the title bar's constructor, destructor), 0x100E17C0 .. 0x100E1850 (constructor, destructor), 0x100E18A0
    (OnCreate), 0x101059E0 (SetTitle).
=============================================================================*/
#include "nwindow/NCRecipeShopWnd.h"
#include "nwindow/NCRecipeShopItemWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NItemC4.h"

enum { DIALOG_RECIPE_PRICE = 0x23 };    // NDialogInfo::ID (see NCRecipeShopItemWnd.cpp)
enum { DIALOG_RECIPE_MESSAGE = 0x24 };  // NDialogInfo::ID (name ours): the shop's message (text); the dialog box answers it
enum { MSG_PriceToBuy = 0x3c3 };        // C4: the same line as the grids' (C3 used 0x142 here)
enum { MSG_ShopMessage = 0x14e };       // systemmsg line the message box shows
enum { SOUND_Close = 6 };               // NConsoleWnd::PlayInterfaceSound (the window closing)

// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12)
enum
{
    STR_RecipeList = 643,               // 0x678B0: the upper grid's label at (11, 32)
    STR_ProductionList = 661,           // 0x67988: the lower one's at (11, 198)
    STR_StartButton = 0x1AC,
    STR_MessageButton = 0x180,
    STR_QuitButton = 0x181,
};

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

/*-----------------------------------------------------------------------------
    The title bar.
-----------------------------------------------------------------------------*/

// 0x100efc60 (vslot 56, ret 4) - the title given by a system string goes to the title bar of this window, ShopFrame (+0x124; the
// base's FrameCtrl stays empty). The stock is `mov ecx, [ecx + 0x124]; jmp NCFrameCtrl::SetTitleID`, no null test
void NCRecipeShopWnd::SetFrameTitleID(INT ID)
{
    ShopFrame->SetTitleID(ID);
}
NW_HOOK_METHOD(0x100efc60, NCRecipeShopWnd, SetFrameTitleID)

// 0x100f2880 - NCFrameCtrl's default constructor, then this class's vtable
NCRecipeShopFrameCtrl::NCRecipeShopFrameCtrl()
{
}
NW_HOOK_CTOR(0x100f2880, NCRecipeShopFrameCtrl)

// 0x100f28d0
NW_HOOK_DTOR(0x100f28d0, NCRecipeShopFrameCtrl)

NW_CHECK_VTABLE(NCRecipeShopFrameCtrl, 0x101c6f08, 79)

// 0x1000f900 (vslot 25, ret 0xC): the stock body is NCResizeFrame's (NCFrameCtrl's with the result as 0 / 1, hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeShopFrameCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x100b14c0)
#else
INT NCRecipeShopFrameCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCFrameCtrl::WindowProc(Msg, wParam, lParam) != 0;   // 0x100b14c0
}
#endif

// 0x100efae0 (vslot 76) - the server is told the shop setup is left, then the frame's close (a tail call, its result is kept)
INT NCRecipeShopFrameCtrl::OnClose()
{
    GConsole->RequestRecipeShopManageQuit();
    return NCFrameCtrl::OnClose();
}
NW_HOOK_METHOD(0x100efae0, NCRecipeShopFrameCtrl, OnClose)

/*-----------------------------------------------------------------------------
    The window.
-----------------------------------------------------------------------------*/

// 0x100f2a00 - the grids, the buttons and the two spare numbers cleared (+0xC8 .. +0xF4)
NCRecipeShopWnd::NCRecipeShopWnd()
{
    RecipeItems = NULL;
    ProductionItems = NULL;
    Unknown0A0 = 0;
    BackTex = NULL;
    Unknown0A8 = 0;
    UpButton = NULL;
    DownButton = NULL;
    StartButton = NULL;
    MessageButton = NULL;
    QuitButton = NULL;
    Unknown0C0 = 0;
    Unknown0C4 = 0;
}
NW_HOOK_CTOR(0x100f2a00, NCRecipeShopWnd)

// 0x100f2a90
NW_HOOK_DTOR(0x100f2a90, NCRecipeShopWnd)

// 0x100efc20 - both grids emptied (the stock code tail-calls the second Clear)
__declspec(noinline) void NCRecipeShopWnd::ClearLists()
{
    RecipeItems->Clear();
    ProductionItems->Clear();
}
NW_HOOK_METHOD(0x100efc20, NCRecipeShopWnd, ClearLists)

// 0x100efb00 (vslot 25) - the buttons' handlers
INT NCRecipeShopWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_RECIPESHOP_START)
            return OnStart() != 0;
        if (wParam == NID_RECIPESHOP_UP)
            return OnRemoveProduct() != 0;
        if (wParam == NID_RECIPESHOP_DOWN)
            return OnAddProduct() != 0;
        if (wParam == NID_RECIPESHOP_MESSAGE)
            return OnMessage() != 0;
        if (wParam == NID_RECIPESHOP_QUIT)
            return OnQuit() != 0;
        if (wParam == NID_RECIPESHOP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100efb00, NCRecipeShopWnd, WindowProc)

// 0x100efc50 (vslot 49) - closed with the grids emptied
void NCRecipeShopWnd::HideWindow()
{
    ClearLists();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100efc50, NCRecipeShopWnd, HideWindow)

// 0x100efc70 (vslot 77) - the server is told the setup is cancelled, the window closes emptied (C3: with the sound)
INT NCRecipeShopWnd::OnCancel()
{
    GConsole->RequestRecipeShopManageCancel();
    ClearLists();
    HideWindow();
    GConsole->PlayInterfaceSound(SOUND_Close);
    return 1;
}
NW_HOOK_METHOD(0x100efc70, NCRecipeShopWnd, OnCancel)

// 0x100efcb0 (vslot 78) - the same for the close button: the server is told the shop is left
INT NCRecipeShopWnd::OnQuit()
{
    GConsole->RequestRecipeShopManageQuit();
    ClearLists();
    HideWindow();
    GConsole->PlayInterfaceSound(SOUND_Close);
    return 1;
}
NW_HOOK_METHOD(0x100efcb0, NCRecipeShopWnd, OnQuit)

// 0x100eff90 (vslot 62) - the background and the two grid labels (C3: DrawTexture, lighter text); C4 adds the "(n/20)" count of
// the production list at the right of its label (5 pixels in, gold)
INT NCRecipeShopWnd::OnPaint(NCGDevice* Device)
{
    TCHAR CountText[128];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 381, 0, 0, 256, 381, BackTex, 0xFF, 1);
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_RecipeList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 198, 0xFFDCDCDC, *SysString(STR_ProductionList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appSprintf(CountText, TEXT("(%d/%d)"), ProductionItems->Items.Num(), 20);
    NCGDevice::GetTextExtent(&Size, CountText, 0, 0, 0, 0, 0, L2FONT_Normal);
    Device->DrawText((INT)(Width - (FLOAT)Size.cx - 5.0f), 198, 0xFFB09B79, CountText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                     0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100eff90, NCRecipeShopWnd, OnPaint)

// 0x100f0130 (vslot 38)
INT NCRecipeShopWnd::OnDestroy()
{
    DestroyChild(this, ShopFrame);
    DestroyChild(this, RecipeItems);
    DestroyChild(this, ProductionItems);
    DestroyChild(this, UpButton);
    DestroyChild(this, DownButton);
    DestroyChild(this, StartButton);
    DestroyChild(this, QuitButton);
    DestroyChild(this, MessageButton);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100f0130, NCRecipeShopWnd, OnDestroy)

// 0x100f02b0 (vslot 76) - Start: the shop's list goes to the server (the number of entries, then the recipe's object id
// and the price of each; the parameter stack is made for three values an entry), the window closes emptied
INT NCRecipeShopWnd::OnStart()
{
    INT Num = ProductionItems->Items.Num();
    L2ParamStack Params(Num * 3 + 1);
    Params.PushBack((void*)(INT_PTR)Num);
    for (INT i = 0; i < ProductionItems->Items.Num(); i++)
    {
        Params.PushBack((void*)(INT_PTR)AsC4(ProductionItems->Items(i))->ObjectID);
        Params.PushBack((void*)(INT_PTR)AsC4(ProductionItems->Items(i))->UnknownC20);
    }
    GConsole->RequestRecipeShopListSet(Params);
    ClearLists();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100f02b0, NCRecipeShopWnd, OnStart)

// 0x100f03a0 (vslot 75) - the down arrow: the price of the selected recipe is asked (the answer is the upper grid's
// OnDialogResult, which moves the recipe to the lower grid)
INT NCRecipeShopWnd::OnAddProduct()
{
    INT Sel = RecipeItems->GetSelected();
    if (Sel >= 0 && Sel < RecipeItems->Items.Num())
    {
        NItemEntry* Item = RecipeItems->Items(Sel);
        if (Item)
        {
            TCHAR Name[64];
            appSprintf(Name, TEXT("%s"), Item->GetName());
            NDialogInfo Info;
            Info.ID = DIALOG_RECIPE_PRICE;
            Info.Type = 3;
            Info.Owner = RecipeItems->Entry;
            Info.bMainBox = 1;
            Info.bInput = 1;
            Info.bLargeAmount = 1;   // C4: the price may reach the large limit
            Info.SetSystemMessage(MSG_PriceToBuy);
            Info.Flag18 = AsC4(Item)->ID;   // the recipe's id
            GConsole->ShowDialog(&Info);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100f03a0, NCRecipeShopWnd, OnAddProduct)

// 0x100f04c0 (vslot 79) - Message: a text box with the shop's message so far (the player's pawn +0x16B4), no owner: the
// dialog box answers it by its ID
INT NCRecipeShopWnd::OnMessage()
{
    NDialogInfo Info;
    Info.ID = DIALOG_RECIPE_MESSAGE;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_ShopMessage);
    Info.Type = 0;
    const TCHAR* Message = NULL;
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        APawn* MyPawn = Me->Pawn;
        if (MyPawn)
            Message = *nw::Field<FString>(MyPawn, 0x16B4);   // (name ours) the shop message; C4: the stock's lea ecx,[eax+0x16B4]
    }
    Info.SetText60(Message);
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x100f04c0, NCRecipeShopWnd, OnMessage)

// 0x100f1b20 (vslot 74) - the up arrow: the selected entry of the lower grid is taken off it
INT NCRecipeShopWnd::OnRemoveProduct()
{
    INT Sel = ProductionItems->GetSelected();
    if (Sel >= 0 && Sel < ProductionItems->Items.Num())
        ProductionItems->Items.Remove(Sel, 1);
    return 1;
}
NW_HOOK_METHOD(0x100f1b20, NCRecipeShopWnd, OnRemoveProduct)

// 0x100f1b10 - what the console forwards (stock: the upper grid's AddItem as a tail jump)
UBOOL NCRecipeShopWnd::AddRecipeItem(NItemInfo* Info)
{
    return RecipeItems->AddItem(Info);
}
NW_HOOK_METHOD(0x100f1b10, NCRecipeShopWnd, AddRecipeItem)

// 0x100f1b00 - the same for the lower grid
UBOOL NCRecipeShopWnd::AddProductionItem(NItemInfo* Info)
{
    return ProductionItems->AddItem(Info);
}
NW_HOOK_METHOD(0x100f1b00, NCRecipeShopWnd, AddProductionItem)

// 0x10034b90: NCWnd::SetTitle's body (the frame control's title text, when there is one and the text is not empty)
#if NW_HOOK_MODE
NW_NAKED void NCRecipeShopWnd::SetTitle(const TCHAR* Title) NW_STOCK(0x10034b90)
#else
// standalone: the stock slot 57 of this class holds NCWnd::SetTitle (0x10034b90) itself, so the override is an alias of it
NW_SLOT_IS("?SetTitle@NCRecipeShopWnd@@UAEXPBG@Z", "?SetTitle@NCWnd@@UAEXPBG@Z")   // NCRecipeShopWnd::SetTitle = NCWnd::SetTitle (0x10034b90)
#endif

// 0x100f2ae0 (vslot 37) - the background, the title bar (id 0xF3), the two grids (6 columns, 4 rows, 160 entries of 32
// pixels, gaps 5 and 3; the upper one takes the recipes back, the lower one asks the price), the arrows and the
// Quit / Message / Start buttons
INT NCRecipeShopWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.RecipeWnd.Recipe1_Back"));

    ShopFrame = new NCRecipeShopFrameCtrl;
    FrameID = NID_RECIPESHOP_FRAME;
    ShopFrame->ID = NID_RECIPESHOP_FRAME;
    ShopFrame->SetPosition(0, 0, 256, 20);
    CreateChild(ShopFrame, NWS_VISIBLE);

    RecipeItems = new NCRecipeShopItemWnd(6, 4, 0xA0, 0x20, 5, 3);
    RecipeItems->SetPosition(9, 48, 239, 139);
    CreateChild(RecipeItems, NWS_VISIBLE | 0x20);
    RecipeItems->SetMode(1);

    ProductionItems = new NCRecipeShopItemWnd(6, 4, 0xA0, 0x20, 5, 3);
    ProductionItems->SetPosition(9, 215, 239, 139);
    CreateChild(ProductionItems, NWS_VISIBLE | 0x20);
    ProductionItems->SetMode(0);

    ProductionItems->SetTarget(RecipeItems);
    RecipeItems->SetTarget(ProductionItems);

    UpButton = new NCButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"), 0);
    UpButton->ID = NID_RECIPESHOP_UP;
    UpButton->SetPosition(112, 194, 15, 15);
    CreateChild(UpButton, NWS_VISIBLE);

    DownButton = new NCButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"), 0);
    DownButton->ID = NID_RECIPESHOP_DOWN;
    DownButton->SetPosition(130, 194, 15, 15);
    CreateChild(DownButton, NWS_VISIBLE);

    QuitButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    QuitButton->ID = NID_RECIPESHOP_QUIT;
    QuitButton->SetPosition(11, 372, 76, 23);
    CreateChild(QuitButton, NWS_VISIBLE);
    QuitButton->SetTextID(STR_QuitButton);

    MessageButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    MessageButton->ID = NID_RECIPESHOP_MESSAGE;
    MessageButton->SetPosition(91, 372, 76, 23);
    CreateChild(MessageButton, NWS_VISIBLE);
    MessageButton->SetTextID(STR_MessageButton);

    StartButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    StartButton->ID = NID_RECIPESHOP_START;
    StartButton->SetPosition(171, 372, 76, 23);
    CreateChild(StartButton, NWS_VISIBLE);
    StartButton->SetTextID(STR_StartButton);
    return 1;
}
NW_HOOK_METHOD(0x100f2ae0, NCRecipeShopWnd, OnCreate)

NW_CHECK_VTABLE(NCRecipeShopWnd, 0x101c7198, 80)
