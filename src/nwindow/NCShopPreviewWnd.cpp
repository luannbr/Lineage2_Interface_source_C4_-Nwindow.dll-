/*=============================================================================
    NCShopPreviewWnd.cpp - the "try on" shop window (a copy of NCShopWnd's code with its own labels and its own OK).
    Stock (C3): 0x100ef900 .. 0x100f006f (WindowProc, SetOwnerID, OnPaint, OnOK, the box's answer, constructor,
    destructor). The window procedure's base is NCStoreWnd's (0x100f0070, shared with NCShopWnd's); SetOwnerID
    (0x100efa00) is one body with the private stores' setters of their +0x104.
=============================================================================*/
#include "nwindow/NCShopPreviewWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NItemC4.h"

enum { DIALOG_PREVIEW_ASK = 0x47 };  // NDialogInfo::ID (name ours): the question OnOK asks
enum { MSG_PreviewAsk = 0x485 };     // systemmsg

// sysstring labels
enum
{
    STR_Adena = 134,
    STR_PreviewList = 811,           // the NPC's list, on top
    STR_PreviewTry = 812,            // the items to try on, below
    STR_PreviewPrice = 813,          // the total's label
};

// 0x101027b0
NCShopPreviewWnd::NCShopPreviewWnd()
{
}
NW_HOOK_CTOR(0x101027b0, NCShopPreviewWnd)

// 0x10102800 - the shop's destructor (0x100a4500) follows
NW_HOOK_DTOR(0x10102800, NCShopPreviewWnd)

// 0x101020f0 (vslot 25) - the shop's procedure (0x100f0070); then the box's answer (the virtual at slot 73), and the
// buttons again (never reached: the shop's procedure has answered them)
INT NCShopPreviewWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCShopWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_DIALOG_RESULT)
        return ::nw::VirtualCall<INT>(this, 79, wParam, lParam) != 0;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_STORE_OK)
            return OnOK() != 0;
        if (wParam == NID_STORE_CANCEL)
            return OnCancel() != 0;
        // slots 70, 71 and 72: the shop's OnUp / OnDown / OnClose (declared as placeholders in NCShopWnd.h)
        if (wParam == NID_STORE_UP)
            return ::nw::VirtualCall<INT>(this, 76) != 0;
        if (wParam == NID_STORE_DOWN)
            return ::nw::VirtualCall<INT>(this, 77) != 0;
        if (wParam == NID_STORE_CLOSE)
            return ::nw::VirtualCall<INT>(this, 78) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x101020f0, NCShopPreviewWnd, WindowProc)

// 0x101021f0 - the NPC's object id (the private stores' windows keep the same setter for their +0x104)
void NCShopPreviewWnd::SetOwnerID(INT InOwnerID)
{
    OwnerID = InOwnerID;
}
NW_HOOK_METHOD(0x101021f0, NCShopPreviewWnd, SetOwnerID)

// 0x10102200 (vslot 62) - the shop's painting with the preview's labels (in its mode only); the total (64 bits, drawn
// with "%d": its low half) and the adena at 247 as the shop's
INT NCShopPreviewWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Text[128];
    TCHAR TotalText[128];
    TCHAR AdenaText[128];
    SIZE LabelSize;
    SIZE Size;
    guard(NCShopPreviewWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    if (Mode == SHOPMODE_Preview)
    {
        Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_PreviewList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(11, 198, 0xFFDCDCDC, *SysString(STR_PreviewTry), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&LabelSize, *SysString(STR_PreviewPrice), 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(156 - LabelSize.cx, 331, 0xFFDCDCDC, *SysString(STR_PreviewPrice), 0, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&LabelSize, *SysString(STR_Adena), 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(156 - LabelSize.cx, 350, 0xFFDCDCDC, *SysString(STR_Adena), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);
    }
    appSprintf(Text, TEXT("%d"), TotalPrice);
    FormatNumber(TotalText, Text);
    appSprintf(Text, TEXT("%s"), TotalText);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    Device->DrawText(247 - Size.cx, 331, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT("%d"), Adena);
    FormatNumber(AdenaText, Text);
    appSprintf(Text, TEXT("%s"), AdenaText);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    Device->DrawText(247 - Size.cx, 350, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10102200, NCShopPreviewWnd, OnPaint)

// 0x101025a0 (vslot 74) - something to try on: a message box asks first (its answer is OnDialogResult)
INT NCShopPreviewWnd::OnOK()
{
    if (Mode == SHOPMODE_Preview && PlayerItems->Items.Num() >= 1)
    {
        NDialogInfo Info;
        Info.Owner = Entry;
        Info.ID = DIALOG_PREVIEW_ASK;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.SetSystemMessage(MSG_PreviewAsk);
        GConsole->ShowDialog(&Info);
    }
    return 1;
}
NW_HOOK_METHOD(0x101025a0, NCShopPreviewWnd, OnOK)

// 0x10102650 (vslot 79) - the box answered: the lower grid's item ids go to the server (the NPC's object id, the list
// id, the count, the ids), and the window closes. Nothing is sent, and the window stays, when the grid holds nothing.
INT NCShopPreviewWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    if (Mode == SHOPMODE_Preview)
    {
        INT Num = 0;
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
            if (AsC4(PlayerItems->Items(i))->TradeCount > 0)
                Num++;
        if (Num == 0)
            return 1;
        L2ParamStack Params(Num + 3);
        Params.PushBack((void*)(INT_PTR)OwnerID);
        Params.PushBack((void*)(INT_PTR)ListID);
        Params.PushBack((void*)(INT_PTR)Num);
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
        {
            if (AsC4(PlayerItems->Items(i))->TradeCount > 0)
                Params.PushBack((void*)(INT_PTR)AsC4(PlayerItems->Items(i))->ID);
        }
        GConsole->RequestPreviewItem(Params);
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10102650, NCShopPreviewWnd, OnDialogResult)

NW_CHECK_VTABLE(NCShopPreviewWnd, 0x101cafd8, 80)
