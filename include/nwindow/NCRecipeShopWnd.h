#pragma once
// NCRecipeShopWnd - the dwarven recipe shop's manage window (UClass "CRecipeShopWnd", 0x100; new in C2; the
// desktop's NCConsole +0x178). The player sets up a private workshop here: the upper grid ("Recipe List")
// holds the recipe book, the lower one ("Production List") the recipes put on sale; moving a recipe down asks its
// price (NCRecipeShopItemWnd.h), the up arrow takes the selected one off the lower grid. Start sends the list
// (RequestRecipeShopListSet: count, then recipe object id and price of each), Message asks the shop's text (the
// dialog box answers with NDialogInfo ID 0x24), and closing the window tells the server to quit (Quit) or to cancel
// the setup (Cancel). C3: the title bar is a class of its own (NCRecipeShopFrameCtrl), whose close button asks the
// server to quit.
//
// NWM_COMMAND ids (names ours): the buttons were made in OnCreate, the ids WindowProc dispatches are the buttons'.
//
// Layout from the C3 constructor 0x100E17C0 (zeroes +0xC8 .. +0xF4), OnCreate 0x100E18A0 and OnDestroy 0x100DEF70.

#include "nwindow/NCWnd.h"
#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCRecipeShopItemWnd;
struct NItemInfo;

enum
{
    NID_RECIPESHOP_FRAME = 0xF3,     // the title bar
    NID_RECIPESHOP_START = 0xF4,     // sends the list (OnStart)
    NID_RECIPESHOP_UP = 0xF6,        // takes the selected product off the lower grid (OnRemoveProduct)
    NID_RECIPESHOP_DOWN = 0xF7,      // asks the price of the selected recipe (OnAddProduct)
    NID_RECIPESHOP_CANCEL = 0xF8,    // no control creates it here (OnCancel)
    NID_RECIPESHOP_MESSAGE = 0xF9,   // asks the shop's message (OnMessage)
    NID_RECIPESHOP_QUIT = 0xF0,      // (OnQuit)
};

// The title bar of the shop's windows (UClass "CRecipeShopFrameCtrl", 0x238, C3 only; the frame of NCRecipeShopWnd): the
// close button also tells the server that the shop setup is left (RequestRecipeShopManageQuit)
class NCRecipeShopFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCRecipeShopFrameCtrl, NCFrameCtrl, 0, 0x102FC680)

    NCRecipeShopFrameCtrl();                                         // 0x100f2880 (NCFrameCtrl's default one)
    // ~NCRecipeShopFrameCtrl                                        // 0x100f28d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (NCResizeFrame's body, shared; a stub here)
    virtual INT OnClose();                                           // 76 0x100efae0
};
NW_ASSERT_SIZE(NCRecipeShopFrameCtrl, 0x26C);

class NCRecipeShopWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeShopWnd, NCWnd, 0, 0x102FD0E0)

    NCRecipeShopItemWnd* RecipeItems;     // +0xE8 (name ours) upper grid, Mode 1: the recipe book
    NCRecipeShopItemWnd* ProductionItems; // +0xEC (name ours) lower grid, Mode 0: what is put on sale
    INT Unknown0A0;                       // +0xF0 (name ours) the constructor clears it, nothing else touches it
    UTexture* BackTex;                    // +0xF4 "L2UI_CH3.RecipeWnd.Recipe1_Back"
    INT Unknown0A8;                       // +0xF8 (cleared by the constructor, never used here)
    NCButton* UpButton;                   // +0xFC (name ours) ScrollBarUpBtn, id 0xF6
    NCButton* DownButton;                 // +0x100 ScrollBarDownBtn, id 0xF7
    NCButton* StartButton;                // +0x104 btn1_normal, id 0xF4, label sysstring 0x1AC
    NCButton* MessageButton;              // +0x108 id 0xF9, label 0x180
    NCButton* QuitButton;                 // +0x10C id 0xF0, label 0x181
    INT Unknown0C0;                       // +0x110 (cleared by the constructor)
    INT Unknown0C4;                       // +0x114 (name ours) the console's recipe shop message (NConsoleWnd vfn126, 0x10059220)
                                          //   stores two numbers here and at +0xF8 before it shows the window; nothing here reads them
    INT Unknown0C8;                       // +0x118 (name ours) see +0xF4 (the constructor leaves it as allocated)   // C2: offset derived, no evidence
    BYTE _Unknown11C[0x124 - 0x11C]; // +0x11C (new in C2)
    NCRecipeShopFrameCtrl* ShopFrame;     // +0x124 (name ours) the title bar (id 0xF3, also NCWnd::FrameID); SetTitle gives it the
                                          //   title, OnDestroy frees it first (C2: not a frame, a button the constructor cleared)

    NCRecipeShopWnd();                                               // 0x100f2a00
    // ~NCRecipeShopWnd                                              // 0x100f2a90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100efb00
    virtual INT OnCreate();                                          // 37 0x100f2ae0
    virtual INT OnDestroy();                                         // 38 0x100f0130
    // ShowWindow (44): 0x100412C0, the body of NCEditBox::ShowWindow (shared, hooked there)
    virtual void HideWindow();                                       // 49 0x100efc50
    virtual void SetTitle(const TCHAR* Title);                       // 57 0x10034b90 (ret 4; shared with another window's)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100eff90
    // names ours: the buttons' handlers
    virtual INT OnRemoveProduct();                                   // 74 0x100f1b20 up arrow
    virtual INT OnAddProduct();                                      // 75 0x100f03a0 down arrow
    virtual INT OnStart();                                           // 76 0x100f02b0
    virtual INT OnCancel();                                          // 77 0x100efc70
    virtual INT OnQuit();                                            // 78 0x100efcb0
    virtual INT OnMessage();                                         // 79 0x100f04c0

    // names ours (the console calls them)
    __declspec(noinline) void ClearLists();                          // 0x100efc20 both grids emptied
    UBOOL AddRecipeItem(NItemInfo* Info);                            // 0x100f1b10 into the upper grid
    UBOOL AddProductionItem(NItemInfo* Info);                        // 0x100f1b00 into the lower grid
    virtual void SetFrameTitleID(INT ID);                            // 56 0x100efc60 (ret 4) ShopFrame (+0x124) takes the SysString title (NCWnd's slot)
    virtual void ShowWindow();  // 48 0x100efc40 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCRecipeShopWnd, RecipeItems, 0xE8);
NW_ASSERT_OFFSET(NCRecipeShopWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCRecipeShopWnd, UpButton, 0xFC);
NW_ASSERT_OFFSET(NCRecipeShopWnd, Unknown0C4, 0x114);
NW_ASSERT_OFFSET(NCRecipeShopWnd, ShopFrame, 0x124);
NW_ASSERT_SIZE(NCRecipeShopWnd, 0x128);

#pragma pack(pop)
