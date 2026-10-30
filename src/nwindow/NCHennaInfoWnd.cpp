/*=============================================================================
    NCHennaInfoWnd.cpp - the confirmation of a tattoo. Stock: 0x10079170 .. 0x100791ee, 0x100791f0 .. 0x1007923a,
    0x10079240 .. 0x100793f9, 0x10079400 .. 0x10079507, 0x10079c60 .. 0x10079e9b, 0x10079ea0 .. 0x1007ab74,
    0x1007abb0 .. 0x1007ac43, 0x1007b230 .. 0x1007b37f.
=============================================================================*/
#include "nwindow/NCHennaInfoWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/NHennaData.h"
#include "nwindow/NItemEntryC4.h"

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
    STR_OK = 140,
    STR_Cancel = 543,
    STR_StatLabel0 = 104,       // the six stat labels (104 .. 109)
    STR_Price = 637,
    STR_Item = 638,
    STR_Symbol = 639,
    STR_Stats = 640,
    STR_AddSymbol = 651,        // the window's title by the mode
    STR_RemoveSymbol = 652,
};

// 0x10093a60 - NCWnd's constructor, the vtable, the two item entries built (Item at +0xF0, Henna at +0x1D30: NItemEntry's
// 0x1001F7F0 each) and the twelve stats (+0x3970 .. +0x399C) cleared; Mode, Adena and the buttons are left as they are (the
// object comes zeroed from the allocator)
NCHennaInfoWnd::NCHennaInfoWnd()
{
    for (INT i = 0; i < 12; i++)
        Stats[i] = 0;
}
NW_HOOK_CTOR(0x10093a60, NCHennaInfoWnd)

// 0x10093b40 - the vtable, ConditionalDestroy, then the tool tips of the two entries (+0x38F0 and +0x1CB0, NToolTipInfo's
// 0x10002E40: the compiler's ~NItemEntry, 0x10050030, is `add ecx, 0x1BC0; jmp 0x10002E40`) and NCWnd's destructor
NW_HOOK_DTOR(0x10093b40, NCHennaInfoWnd)


// 0x10091860 (vslot 25) - OK and Cancel
INT NCHennaInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_HENNAINFO_OK)
            return OnOK() != 0;
        if (wParam == NID_HENNAINFO_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10091860, NCHennaInfoWnd, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10093420 (vslot 37) - C3: the title bar is as wide as the window, the two textures and the buttons (Cancel first)
INT NCHennaInfoWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_HENNAINFO_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_ch3.HennaWnd.Henna2_back"));

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_HENNAINFO_CANCEL;
    CancelButton->SetPosition(51, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE | 0x20);   // 0x20: meaning unknown
    CancelButton->SetTextID(STR_Cancel);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_HENNAINFO_OK;
    OKButton->SetPosition(131, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE | 0x20);
    OKButton->SetTextID(STR_OK);
    return 1;
}
NW_HOOK_METHOD(0x10093420, NCHennaInfoWnd, OnCreate)

// 0x100930e0 (vslot 38) - the base first, then Cancel and OK
INT NCHennaInfoWnd::OnDestroy()
{
    guard(NCHennaInfoWnd::OnDestroy);
    NCWnd::OnDestroy();
    DestroyChild(this, CancelButton);
    DestroyChild(this, OKButton);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100930e0, NCHennaInfoWnd, OnDestroy)

// the tattoo's entry
static FORCEINLINE NItemEntry* HennaOf(NCHennaInfoWnd* Wnd)
{
    return &Wnd->Henna;
}

// 0x10091bb0 (vslot 48) - the title follows the mode (slot 56 takes a system string id in C4: NCWnd's title bar's)
void NCHennaInfoWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Mode == 0)
        ::nw::VirtualCall<void>(this, 56, STR_AddSymbol);
    else if (Mode == 1)
        ::nw::VirtualCall<void>(this, 56, STR_RemoveSymbol);
}
NW_HOOK_METHOD(0x10091bb0, NCHennaInfoWnd, ShowWindow)

// 0x10091b00 (vslot 74) - OK draws (Mode 0) or takes off (Mode 1) the tattoo, then the window closes
INT NCHennaInfoWnd::OnOK()
{
    if (Mode == 0)
    {
        GConsole->RequestHennaEquip(EntryObjectID(HennaOf(this)));
        HideWindow();
        return 1;
    }
    if (Mode == 1)
        GConsole->RequestHennaUnequip(EntryObjectID(HennaOf(this)));
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10091b00, NCHennaInfoWnd, OnOK)

// 0x10091b60 (vslot 75) - the window closes and the server is asked for the list again
INT NCHennaInfoWnd::OnCancel()
{
    HideWindow();
    if (Mode == 0)
    {
        GConsole->RequestHennaItemList();
        return 1;
    }
    if (Mode == 1)
        GConsole->RequestHennaUnequipList();
    return 1;
}
NW_HOOK_METHOD(0x10091b60, NCHennaInfoWnd, OnCancel)

// 0x100918e0 (name ours) - the twelve stats zeroed
void NCHennaInfoWnd::ClearParams()
{
    for (INT i = 0; i < 12; i++)
        Stats[i] = 0;
}
NW_HOOK_METHOD(0x100918e0, NCHennaInfoWnd, ClearParams)

// a name of an item's data (C4: the L2FNames of FL2ItemDataBase are at +0x3C icon, +0x50 name, +0x54 description; the
// header's NViewItemData has the C2 offsets)
static FORCEINLINE const TCHAR* ItemDataText(void* ItemData, INT Offset)
{
    return **(L2FName*)((BYTE*)ItemData + Offset);
}

// 0x10091930 (ret 4, name ours) - the dye's entry from the item data (name, description, icon name; the price and the
// count from the parameters), the tattoo's from the henna data, then the twelve stats; the icons by name, the black texture
// when there is none
void NCHennaInfoWnd::SetHenna(const NHennaParam* Param)
{
    void* ItemData = GL2GameData.GetItemData(Param->ItemID);
    FL2HennaData* HennaData = GL2GameData.GetHennaData(Param->HennaID);
    if (ItemData && HennaData)
    {
        NItemEntry* Dye = &Item;
        NItemEntry* Tattoo = HennaOf(this);
        appStrcpy(EntryName(Dye), ItemDataText(ItemData, 0x50));
        appStrcpy(EntryDescription(Dye), ItemDataText(ItemData, 0x54));
        appStrcpy(EntryIconName(Dye), ItemDataText(ItemData, 0x3C));
        EntryPrice(Dye) = Param->Price;
        EntryCount(Dye) = Param->Count;
        if (EntryIconName(Dye))
            EntryIcon(Dye) = FindTexture(EntryIconName(Dye), 1);
        if (!EntryIcon(Dye))
            EntryIcon(Dye) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
        appStrcpy(EntryName(Tattoo), *HennaData->Name);
        appStrcpy(EntryDescription(Tattoo), *HennaData->Description);
        appStrcpy(EntryIconName(Tattoo), *HennaData->IconName);
        EntryObjectID(Tattoo) = Param->HennaID;
        if (EntryIconName(Tattoo))
            EntryIcon(Tattoo) = FindTexture(EntryIconName(Tattoo), 1);
        if (!EntryIcon(Tattoo))
            EntryIcon(Tattoo) = FindTexture(TEXT("NWindow.BlackTexture"), 1);
        Stats[0] = Param->Stats[0];
        Stats[1] = Param->Stats[1];
        Stats[2] = Param->Stats[2];
        Stats[3] = Param->Stats[3];
        Stats[4] = Param->Stats[4];
        Stats[5] = Param->Stats[5];
        Stats[6] = Param->Stats[6];
        Stats[7] = Param->Stats[7];
        Stats[8] = Param->Stats[8];
        Stats[9] = Param->Stats[9];
        Stats[10] = Param->Stats[10];
        Stats[11] = Param->Stats[11];
    }
}
NW_HOOK_METHOD(0x10091930, NCHennaInfoWnd, SetHenna)

// "now > new" of a stat: the numbers centered on X1 and X2, the ">" at XSep
static FORCEINLINE void DrawStatPair(NCGDevice* Device, INT X1, INT XSep, INT X2, INT Y, INT Now, INT New)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, appItoa(Now), 0, 0, 0, 0, 0);
    Device->DrawText(X1 - Size.cx / 2, Y, 0xFFB09B79, appItoa(Now), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(XSep, Y, 0xFFB09B79, TEXT(">"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, appItoa(New), 0, 0, 0, 0, 0);
    Device->DrawText(X2 - Size.cx / 2, Y, 0xFFB09B79, appItoa(New), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// 0x100922f0 (vslot 62) - the background under the title bar, the two items (the dye first when drawing, the tattoo first
// when taking off) with their descriptions (the dye's name wraps at the window's width less 60), the six stats "now > new"
// in two columns of three rows, and the adena. The texts are light grey (DCDCDC)
INT NCHennaInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCHennaInfoWnd::OnPaint);
    TCHAR Text[260];
    SIZE Size;
    NItemEntry* Dye = &Item;
    NItemEntry* Tattoo = HennaOf(this);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex,
                            0xFF, 1);
    if (Mode == 0)
    {
        FString Desc;
        GConsole->ConvertNewLines(EntryDescription(Dye), Desc);
        Device->DrawText(18, 43, 0xFFDCDCDC, *GL2GameData.SysString[STR_Item], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (EntryIcon(Dye))
            Device->DrawTexture(16, 71, 32, 32, 0, 0, 32, 32, EntryIcon(Dye), 0xFF, 1);
        Device->DrawText(58, 66, 0xFFDCDCDC, EntryName(Dye), (DWORD)(INT)(Width - 60.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
        Device->DrawText(58, 80, 0xFFDCDCDC, *Desc, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        appSprintf(Text, TEXT("%s %d"), *GL2GameData.SysString[STR_Price], EntryPrice(Dye));
        Device->DrawText(58, 94, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(18, 132, 0xFFDCDCDC, *GL2GameData.SysString[STR_Symbol], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (EntryIcon(Tattoo))
            Device->DrawTexture(16, 161, 32, 32, 0, 0, 32, 32, EntryIcon(Tattoo), 0xFF, 1);
        Device->DrawText(58, 156, 0xFFDCDCDC, EntryName(Tattoo), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        GConsole->ConvertNewLines(EntryDescription(Tattoo), Desc);
        Device->DrawText(58, 170, 0xFFDCDCDC, *Desc, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    else if (Mode == 1)
    {
        FString Desc;
        Device->DrawText(18, 43, 0xFFDCDCDC, *GL2GameData.SysString[STR_Symbol], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (EntryIcon(Tattoo))
            Device->DrawTexture(16, 71, 32, 32, 0, 0, 32, 32, EntryIcon(Tattoo), 0xFF, 1);
        appSprintf(Text, TEXT("%s %s"), *GL2GameData.SysString[STR_RemoveSymbol], EntryName(Tattoo));
        Device->DrawText(58, 66, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        GConsole->ConvertNewLines(EntryDescription(Tattoo), Desc);
        appStrcpy(Text, *Desc);
        Device->DrawText(58, 80, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(18, 132, 0xFFDCDCDC, *GL2GameData.SysString[STR_Item], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (EntryIcon(Dye))
            Device->DrawTexture(16, 161, 32, 32, 0, 0, 32, 32, EntryIcon(Dye), 0xFF, 1);
        Device->DrawText(58, 156, 0xFFDCDCDC, EntryName(Dye), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        GConsole->ConvertNewLines(EntryDescription(Dye), Desc);
        Device->DrawText(58, 170, 0xFFDCDCDC, *Desc, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        appSprintf(Text, TEXT("%s %d"), *GL2GameData.SysString[STR_Price], EntryPrice(Dye));
        Device->DrawText(58, 184, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    Device->DrawText(18, 221, 0xFFDCDCDC, *GL2GameData.SysString[STR_Stats], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(17, 247, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(17, 263, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0 + 1], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(17, 279, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0 + 2], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(135, 247, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0 + 3], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(135, 263, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0 + 4], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(135, 279, 0xFFA3A3A3, *GL2GameData.SysString[STR_StatLabel0 + 5], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    // the left column: cells 1, 4 and 2; the right: 0, 5 and 3
    DrawStatPair(Device, 64, 78, 97, 247, Stats[2], Stats[3]);
    DrawStatPair(Device, 64, 78, 97, 263, Stats[8], Stats[9]);
    DrawStatPair(Device, 64, 78, 97, 279, Stats[4], Stats[5]);
    DrawStatPair(Device, 182, 196, 215, 247, Stats[0], Stats[1]);
    DrawStatPair(Device, 182, 196, 215, 263, Stats[10], Stats[11]);
    DrawStatPair(Device, 182, 196, 215, 279, Stats[6], Stats[7]);
    if (AdenaTex)
        Device->DrawTexture(52, 313, 16, 12, 0, 0, 16, 12, AdenaTex, 0xFF, 1);
    appSprintf(Text, TEXT("%d"), Adena);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText(229 - Size.cx, 329, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100922f0, NCHennaInfoWnd, OnPaint)

NW_CHECK_VTABLE(NCHennaInfoWnd, 0x101b62c0, 76)
