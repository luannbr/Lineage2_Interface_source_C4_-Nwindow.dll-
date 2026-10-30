/*=============================================================================
    NCTrainListBox.cpp - trainer skill list. Stock: 0x100ba9a0 .. 0x100baad5,
    0x100baed0 .. 0x100bb49c, 0x100bc570 .. 0x100bc79f, 0x100bcb40 .. 0x100bcc42; bodies
    shared with NCVIPShopItemListBox at 0x1006bad0 and 0x1006dda0.
=============================================================================*/
#include "nwindow/NCTrainListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NWndCreate.h"

// 0x1011e300 (wave C; the stage 3 judge had demoted it to a forwarder that ran the stock constructor over the object: the array's stores
// were the member's own) - NCListBox's constructor, this class' vtable, no rows (the array's three zero stores), the lines 8 apart
NCTrainListBox::NCTrainListBox()
{
    LineGap = 8;
}
NW_HOOK_CTOR(0x1011e300, NCTrainListBox)

// 0x1011e3c0 (ret 0xC; NCTrainListWnd::OnCreate makes the list with it: 100 lines, 7 shown) - the same with the list box's three numbers, and
// the text one pixel down (+0x234, TextY, which the stock stores after the gap)
NCTrainListBox::NCTrainListBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
    LineGap = 8;
    TextY = 1;
}
NW_HOOK_CTOR_ARGS(0x1011e3c0, NCTrainListBox, (INT A, INT B, INT C), (A, B, C))

// 0x1011e360 - the entries' array (0x100dde60), then NCListBox (its scalar deleting destructor, 0x100ddf20, is slot 3 and
// calls this)
NW_HOOK_DTOR(0x1011e360, NCTrainListBox)


// 0x10020890 (vslot 25) - the stock slot is NCTextListBox's body (hooked there): the plain forward to NCListBox, no enabled check. The C3 body that
// stood here (`Msg != WM_PAINT && !IsEnabled()` first) is not the C4 one; with the constructors hooked (wave C) this is the slot of our vtable,
// so it jumps to the stock body like NCRecipeListBox's
#if NW_HOOK_MODE
NW_NAKED INT NCTrainListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10020890)
#else
INT NCTrainListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;   // 0x10020890: NCTextListBox::WindowProc, the plain forward
}
#endif

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1011b740 (vslot 37) - C3: two outline textures for the icons
INT NCTrainListBox::OnCreate()
{
    NCListBox::OnCreate();
    BackTex = FindTexture(TEXT("sek.cbui315"));
    OutlineDownTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine_down"));
    OutlineTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine"));
    return 1;
}
NW_HOOK_METHOD(0x1011b740, NCTrainListBox, OnCreate)

// 0x1011ddc0 (vslot 38; 97 bytes) - the rows freed and the array emptied, then NCListBox's OnDestroy; 1. The stock body is shared with
// NCRecipeListBox (its rows are at +0x240 too: NCRecipeListBox::OnDestroy is a stub that jumps to this address)
INT NCTrainListBox::OnDestroy()
{
    for (INT i = 0; i < Entries.Num(); i++)
    {
        if (Entries(i))
            appFree(Entries(i));
    }
    Entries.Empty();
    NCListBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x1011ddc0, NCTrainListBox, OnDestroy)

// 0x1011c580 (vslot 62) - icon (inside two outlines), name, "Lv <level>" and, from x = 76, "Required SP : <sp>"; an enchanted
// route's row (C4) shows the route's name instead of those two texts. A NULL entry skips only the icon and the name, as in
// the stock code. The selected row's highlight is stretched from half a gap above the row to the width less 49 (a left part
// and a 32 pixel edge)
INT NCTrainListBox::OnPaint(NCGDevice* Device)
{
    guard(NCTrainListBox::OnPaint);
    // Stock layout: a 16-character buffer followed by the text size. With the English strings
    // the terminator of "Unacquired state" (16 characters) lands in Size.cx, so that text is
    // drawn at TextX + 36 + 0 over "Lv"; kept in one struct so the overflow stays harmless.
    struct
    {
        TCHAR Text[16];
        SIZE Size;
    } T;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Entries.Num(); i++, Line++)
    {
        NTrainEntry* Entry = Entries(i);
        if (i == Selected)
        {
            Device->DrawStretchedTex(0, (LineGap + 32) * Line - LineGap / 2 + TextY, (INT)(Width - 47.0f - 2.0f),
                                     LineGap + 32, 0, 0, 16, 13, HighlightTex, 0xFF);
            Device->DrawStretchedTex((INT)(Width - 47.0f - 2.0f), (LineGap + 32) * Line - LineGap / 2 + TextY, 32,
                                     LineGap + 32, 0, 0, 32, 13, LineTex, 0xFF);
        }
        if (Entry)
        {
            if (Entry->Icon)
            {
                Device->DrawTexture(TextX - 1, (LineGap + 32) * Line + TextY - 1, 34, 34, 0, 0, 34, 34, OutlineDownTex,
                                    0xFF, 0);
                Device->DrawTexture(TextX - 1, (LineGap + 32) * Line + TextY - 1, 35, 35, 0, 0, 35, 35, OutlineTex, 0xFF,
                                    0);
                Device->DrawTexture(TextX, (LineGap + 32) * Line + TextY, 32, 32, 0, 0, 32, 32, Entry->Icon, 0xFF, 1);
            }
            if (Entry->Name[0])
                Device->DrawText(TextX + 34, (LineGap + 32) * Line + TextY + 3, 0xFFDCDCDC, Entry->Name, 0, 0, 0,
                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }

        if (Entry->bEnchanted)
        {
            FL2MagicSkillData* Skill = GL2GameData.GetMSData(Entry->SkillID, Entry->Level);
            if (!Skill)
                continue;
            Device->DrawText(TextX + 34, (LineGap + 32) * Line + TextY + 19, 0xFFB09B79, *Skill->EnchantName, 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            continue;
        }

        // "Lv" (SysString 88), then the level or "Unacquired state" (371)
        if (*GL2GameData.SysString[88])
            appSprintf(T.Text, TEXT("%s"), *GL2GameData.SysString[88]);
        else
            T.Text[0] = 0;
        Device->DrawText(TextX + 34, (LineGap + 32) * Line + TextY + 19, 0xFFA3A3A3, T.Text, 0, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&T.Size, T.Text, 0, 0, 0, 0, 0);
        if (Entry->Level > 0)
            appSprintf(T.Text, TEXT("%d"), Entry->Level);
        else if (*GL2GameData.SysString[371])
            appSprintf(T.Text, TEXT("%s"), *GL2GameData.SysString[371]);
        else
            T.Text[0] = 0;
        Device->DrawText(TextX + T.Size.cx + 36, (LineGap + 32) * Line + TextY + 19, 0xFFB09B79, T.Text, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0);

        // "Required SP" (365), " : " and the SP
        if (*GL2GameData.SysString[365])
            appSprintf(T.Text, TEXT("%s"), *GL2GameData.SysString[365]);
        else
            T.Text[0] = 0;
        Device->DrawText(76, (LineGap + 32) * Line + TextY + 19, 0xFFA3A3A3, T.Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
        NCGDevice::GetTextExtent(&T.Size, T.Text, 0, 0, 0, 0, 0);
        appSprintf(T.Text, TEXT(" : "));
        Device->DrawText(T.Size.cx + 76, (LineGap + 32) * Line + TextY + 19, 0xFFDCDCDC, T.Text, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        if (*GL2GameData.SysString[365])
            appSprintf(T.Text, TEXT("%s : "), *GL2GameData.SysString[365]);
        else
            T.Text[0] = 0;
        NCGDevice::GetTextExtent(&T.Size, T.Text, 0, 0, 0, 0, 0);
        appSprintf(T.Text, TEXT("%d"), Entry->SP);
        Device->DrawText(T.Size.cx + 76, (LineGap + 32) * Line + TextY + 19, 0xFFB09B79, T.Text, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011c580, NCTrainListBox, OnPaint)

// 0x1011b870 (vslot 68)
INT NCTrainListBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected >= 0 && Selected < Entries.Num())
        NotifyParent(Selected);
    return 1;
}
NW_HOOK_METHOD(0x1011b870, NCTrainListBox, OnLButtonDown)

// 0x1011b790 (vslot 75) - NCListBox::HitTest with 32 px rows (and the gap)
INT NCTrainListBox::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0 || RX > (INT)Width || RY < 0 || RY > (INT)Height)
        return -1;
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex; i++, Line++)
    {
        if (i >= NumLines)
            break;
        if (RX < (INT)Width && RY >= (LineGap + 32) * Line + TextY && RY < (Line + 1) * (LineGap + 32) + TextY)
        {
            Selected = i;
            break;
        }
    }
    return Selected;
}
NW_HOOK_METHOD(0x1011b790, NCTrainListBox, HitTest)

// 0x100f1b60 (vslot 77): the rows freed, the scroll bar and the view reset (the body is the same in the list boxes that keep their rows
// at +0x240)
void NCTrainListBox::Clear()
{
    for (INT i = 0; i < Entries.Num(); i++)
    {
        if (Entries(i))
            appFree(Entries(i));
    }
    Entries.Empty();
    ScrollBar->Reset();
    TopIndex = 0;
    NumLines = 0;
    Selected = -1;
}
NW_HOOK_METHOD(0x100f1b60, NCTrainListBox, Clear)

// 0x100c1110 - the list boxes' shared body (hooked on NCRecipeListBox): the array's count into the scroll bar
__declspec(noinline) void NCTrainListBox::UpdateEntryScrollBar()
{
    NumLines = Entries.Num();
    ScrollBar->SetRange(NumLines, VisibleLines);
    ScrollBar->SetValue(TopIndex);
}

// 0x1011de30 - keeps a copy (the stock allocates 0x14A8 bytes and runs the constructor 0x1006E8C0); the icon comes from
// Src's texture name (the black texture when it is not found). Unlike NCListBox::AddString nothing is dropped when full.
void NCTrainListBox::AddEntry(const NTrainEntry* Src)
{
    NTrainEntry* Entry = (NTrainEntry*)appMalloc(sizeof(NTrainEntry), TEXT("new"));
    if (Entry)
    {
#if NW_HOOK_MODE
        Entry = ::nw::StockCallAt<NTrainEntry*>(0x1006E8C0, Entry);   // NTrainEntryExt::Construct (NConsoleWndAnon.cpp), out of line as here
#else
        Entry = new ((void*)Entry) NTrainEntry;   // the same zero / -1 initial values (NTrainEntry's constructor)
#endif
    }
    Entry->Copy(Src);
    if (Src->IconName)
        Entry->Icon = FindTexture(Src->IconName);
    if (!Entry->Icon)
        Entry->Icon = FindTexture(TEXT("NWindow.BlackTexture"));
    if (NumLines < MaxLines)
    {
        NumLines++;
        if (bAutoScroll && NumLines > VisibleLines)
            TopIndex++;
    }
    Entries.AddItem(Entry);
    UpdateEntryScrollBar();
}
NW_HOOK_METHOD(0x1011de30, NCTrainListBox, AddEntry)

// 0x1011cbf0
UBOOL NCTrainListBox::GetSelectedEntry(NTrainEntry* Out)
{
    if (Selected < 0 || Selected >= Entries.Num())
        return 0;
    NTrainEntry* Entry = Entries(Selected);
    if (!Entry)
        return 0;
    Out->Copy(Entry);
    return 1;
}
NW_HOOK_METHOD(0x1011cbf0, NCTrainListBox, GetSelectedEntry)

NW_CHECK_VTABLE(NCTrainListBox, 0x101cf3d8, 82)
