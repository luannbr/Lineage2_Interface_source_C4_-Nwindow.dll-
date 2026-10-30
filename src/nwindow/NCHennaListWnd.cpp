/*=============================================================================
    NCHennaListWnd.cpp - the tattoo lists of the symbol maker. C3 stock: 0x10086340 .. 0x10086e00 (WindowProc, the
    title, OnDestroy, paint, OnSelect), 0x10087fe0, 0x100883b0 .. 0x10088460 (constructor, destructor, OnCreate).
=============================================================================*/
#include "nwindow/NCHennaListWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NGameData.h"

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// sysstring
enum
{
    STR_Price = 637,
    STR_AddSymbol = 651,        // the window's title when it lists the tattoos to draw
    STR_RemoveSymbol = 652,     // ... to take off
    STR_SelectToAdd = 659,      // the heading on the paper
    STR_SelectToRemove = 660,
};

// 0x10093800 - the list box, its texture and the adena are cleared (the mode is left as allocated: the console sets it)
NCHennaListWnd::NCHennaListWnd()
{
    ListBox = NULL;
    BackTex = NULL;
    Adena = 0;
}
NW_HOOK_CTOR(0x10093800, NCHennaListWnd)

// 0x10093860 - nothing of its own: ConditionalDestroy, then the window's
NW_HOOK_DTOR(0x10093860, NCHennaListWnd)


// 0x100917a0 (vslot 25) - a click on a row reports its index
INT NCHennaListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_HENNALIST_LIST)
        return OnSelect((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100917a0, NCHennaListWnd, WindowProc)

// 0x100938b0 (vslot 37) - the title bar, the background and the list box (100 rows kept, 6 seen, 16 pixels each in the
// C2 sense); the list's texts start 15 / 9 pixels in, the rows are 5 pixels apart
INT NCHennaListWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_HENNALIST_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_ch3.HennaWnd.henna1_back"), 1);
    ListBox = new NCHennaListBox(100, 6, 16);
    ListBox->ID = NID_HENNALIST_LIST;
    ListBox->SetPosition(7, 46, 241, 309);
    CreateChild(ListBox, NWS_VISIBLE);
    ListBox->TextX = 15;
    ListBox->TextY = 9;
    ListBox->LineGap = 5;
    return 1;
}
NW_HOOK_METHOD(0x100938b0, NCHennaListWnd, OnCreate)

// 0x10091fd0 (vslot 38) - the list box goes, then the base
INT NCHennaListWnd::OnDestroy()
{
    guard(NCHennaListWnd::OnDestroy);
    DestroyChild(this, ListBox);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10091fd0, NCHennaListWnd, OnDestroy)

// 0x10091820 (vslot 48) - the title follows the mode (slot 56 takes a system string id in C4: NCWnd's title bar's)
void NCHennaListWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Mode == 0)
        ::nw::VirtualCall<void>(this, 56, STR_AddSymbol);
    else if (Mode == 1)
        ::nw::VirtualCall<void>(this, 56, STR_RemoveSymbol);
}
NW_HOOK_METHOD(0x10091820, NCHennaListWnd, ShowWindow)

// 0x10092080 (vslot 62) - the background under the title bar, the heading by the mode and the adena (right aligned at 230)
INT NCHennaListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeListWnd::OnPaint);   // the stock string (the code is copied from that window's)
    TCHAR Buffer[32];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT BackH = (INT)(Height - 20.0f);
        INT BackW = (INT)Width;
        Device->DrawTexture(0, 20, BackW, BackH, 0, 0, BackW, BackH, BackTex, 0xFF, 1);
    }
    if (Mode == 0)
        Device->DrawText(15, 28, 0xFFDCDCDC, *GL2GameData.SysString[STR_SelectToAdd], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0, 0);
    else if (Mode == 1)
        Device->DrawText(15, 28, 0xFFDCDCDC, *GL2GameData.SysString[STR_SelectToRemove], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                         0, 0, 0, 0);
    appSprintf(Buffer, TEXT("%d"), Adena);
    NCGDevice::GetTextExtent(&Size, Buffer, 0, 0, 0, 0, 0);
    Device->DrawText(230 - Size.cx, 374, 0xFFDCDCDC, Buffer, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10092080, NCHennaListWnd, OnPaint)

// 0x10092230 (vslot 74) - a row clicked: the server is asked for the tattoo's information (the list to draw or to take off)
INT NCHennaListWnd::OnSelect(INT Index)
{
    NHennaListEntry Entry;
    if (ListBox->GetSelectedEntry(&Entry))
    {
        if (Mode == 0)
        {
            GConsole->RequestHennaItemInfo(Entry.HennaID);
            return 1;
        }
        if (Mode == 1)
            GConsole->RequestHennaUnequipInfo(Entry.HennaID);
    }
    return 1;
}
NW_HOOK_METHOD(0x10092230, NCHennaListWnd, OnSelect)

// 0x10091800 (name ours) - the rows and the adena cleared
void NCHennaListWnd::Clear()
{
    ListBox->Clear();
    Adena = 0;
}
NW_HOOK_METHOD(0x10091800, NCHennaListWnd, Clear)

// 0x10093410 (name ours) - a row for the list (the stock code jumps into the list box's AddEntry)
void NCHennaListWnd::AddEntry(const NHennaListEntry* Src)
{
    ListBox->AddEntry(Src);
}
NW_HOOK_METHOD(0x10093410, NCHennaListWnd, AddEntry)

NW_CHECK_VTABLE(NCHennaListWnd, 0x101b6158, 75)
