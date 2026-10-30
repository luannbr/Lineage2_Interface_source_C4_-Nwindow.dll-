/*=============================================================================
    NCComboBox.cpp - drop-down list. Stock (C3): 0x10004ce0 .. 0x100059f4.
=============================================================================*/
#include "nwindow/NCComboBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x10006a10 - NControl's constructor is inlined; the rows are 17 high
NCComboBox::NCComboBox()
{
    TextY = 0;
    TextX = 5;
    SelectTex = NULL;
    DropButton = NULL;
    bDropped = 0;
    Selected = -1;
    Hover = -1;
    RowStep = 17;
}
NW_HOOK_CTOR(0x10006a10, NCComboBox)


// 0x10005b00 (vslot 25) - the drop button reports here; C2: no enabled test
INT NCComboBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_COMBO_DROP)
        return DropDown() != 0;
    return 0;
}
NW_HOOK_METHOD(0x10005b00, NCComboBox, WindowProc)

// 0x10005cd0 (vslot 30) - the list closes (the window goes back to its closed size), then the base's
void NCComboBox::KillFocus()
{
    if (bDropped)
    {
        SetWindowSize((FLOAT)ClosedW, (FLOAT)ClosedH);
        bDropped = 0;
    }
    NCWnd::KillFocus();
}
NW_HOOK_METHOD(0x10005cd0, NCComboBox, KillFocus)

// 0x100064d0 (vslot 37) - the box textures (the CH3 set), the row height from "A", the closed size from the local
// rectangle, the drop button 15 x 15 near the right edge
INT NCComboBox::OnCreate()
{
    NControl::OnCreate();
    LeftTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox1"));
    MidTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox2"));
    RightTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox3"));
    LeftDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox1_disable"));
    MidDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox2_disable"));
    RightDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.textbox3_disable"));
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    NCGDevice::GetTextExtent(&RowSize, TEXT("A"), 0, 0, 0, 0, 0, L2FONT_Normal);
    NRectF Rect;
    Rect.X = 0;
    Rect.Y = 0;
    Rect.W = 0.0f;
    Rect.H = 0.0f;
    GetLocalRect(&Rect);
    ClosedW = (INT)Rect.W;
    ClosedH = (INT)Rect.H;
    DropButton = new NCButton(TEXT("L2UI_CH3.Button.downbutton"), TEXT("L2UI_CH3.Button.downbutton_down"), 0);
    DropButton->ID = NID_COMBO_DROP;
    DropButton->SetPosition((INT)(Rect.W - 15.0f - 1.0f), (INT)(Height * 0.5f - 7.0f), 15, 15);
    CreateChild(DropButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100064d0, NCComboBox, OnCreate)

// 0x10006720 (vslot 38, guard) - the drop button, then the items (the texts go with FreeItems)
INT NCComboBox::OnDestroy()
{
    guard(NCComboBox::OnDestroy);
    if (DropButton)
    {
        RemoveChildWindow(DropButton->Entry);
        delete DropButton;
        DropButton = NULL;
    }
    FreeItems();
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10006720, NCComboBox, OnDestroy)

// 0x10005dc0 (anonymous in the db, ret 8) - a SysString item (the id is >= 0) has no text of its own; a text item owns an
// NString made with the global new
NComboItem::NComboItem(INT InTextID, const TCHAR* InText)
{
    TextID = InTextID;
    if (InTextID < 0 && InText)
        Text = new NString(InText);
    else
        Text = NULL;
}
NW_HOOK_CTOR_ARGS(0x10005dc0, NComboItem, (INT A, const TCHAR* B), (A, B))

// 0x10005b60 (anonymous in the db) - the SysString of the item, or its text (NULL when there is neither)
const TCHAR* NComboItem::GetText()
{
    if (TextID >= 0)
        return *NCObject::SysString(TextID);
    if (Text)
        return Text->GetAt(0);
    return NULL;
}
NW_HOOK_METHOD(0x10005b60, NComboItem, GetText)

// 0x100064b0 (anonymous in the db) - the NString goes (destroyed and freed; the pointer is not cleared)
void NComboItem::DeleteText()
{
    if (Text)
        delete Text;
}
NW_HOOK_METHOD(0x100064b0, NComboItem, DeleteText)

// 0x100066d0 (anonymous in the db, name ours) - the texts of the items go, then the array
void NCComboBox::FreeItems()
{
    for (INT i = 0; i < Items.Num(); i++)
        Items(i).DeleteText();
    Items.Empty();
}
NW_HOOK_METHOD(0x100066d0, NCComboBox, FreeItems)

// 0x100067d0 (anonymous in the db) - the array is emptied (the stock code does not free the texts here) and the selection goes
void NCComboBox::ClearItems()
{
    Items.Empty();
    Selected = -1;
    Hover = -1;
}
NW_HOOK_METHOD(0x100067d0, NCComboBox, ClearItems)

// 0x10006800 (anonymous in the db, ret 4) - a SysString as an item (nothing for a negative id)
void NCComboBox::AddItem(INT SysStringID)
{
    if (SysStringID >= 0)
    {
        NComboItem Item(SysStringID, NULL);
        Items.AddItem(Item);
    }
}
NW_HOOK_METHOD_SIG(0x10006800, NCComboBox, AddItem, void (NCComboBox::*)(INT))

// 0x10006850 (anonymous in the db, ret 8; name ours) - a SysString as an item with its data (nothing for a negative id): the same as the text's
// with the data setter 0x10005BA0 (the linker folded it with every other setter of a dword at +8)
void NCComboBox::AddSysStringItem(INT SysStringID, INT Data)
{
    if (SysStringID >= 0)
    {
        NComboItem Item(SysStringID, NULL);
        Item.Data = Data;   // 0x10005BA0 in the stock: the setter of a dword at +8 (folded with every other one)
        Items.AddItem(Item);
    }
}
NW_HOOK_METHOD(0x10006850, NCComboBox, AddSysStringItem)

// 0x100068b0 (anonymous in the db, ret 4) - a text as an item (nothing when it is empty)
void NCComboBox::AddItem(const TCHAR* Text)
{
    if (Text && *Text)
    {
        NComboItem Item(-1, Text);
        Items.AddItem(Item);
    }
}
NW_HOOK_METHOD_SIG(0x100068b0, NCComboBox, AddItem, void (NCComboBox::*)(const TCHAR*))

// 0x10006910 (anonymous in the db, ret 8) - a text as an item, with its data (the dword at +8 is set by the setter the linker folded
// with every other one of a dword at +8)
void NCComboBox::AddItem(const TCHAR* Text, INT Data)
{
    if (Text && *Text)
    {
        NComboItem Item(-1, Text);
        Item.Data = Data;   // 0x10005BA0 in the stock: the setter of a dword at +8 (folded with every other one)
        Items.AddItem(Item);
    }
}
NW_HOOK_METHOD_SIG(0x10006910, NCComboBox, AddItem, void (NCComboBox::*)(const TCHAR*, INT))

// 0x100063e0 (ret 4) - the item's text, NULL out of range
const TCHAR* NCComboBox::GetItemText(INT Index)
{
    if (Index >= 0 && Index < Items.Num())
        return Items(Index).GetText();
    return NULL;
}
NW_HOOK_METHOD(0x100063e0, NCComboBox, GetItemText)

// 0x10005e40 (vslot 56) - the closed box (three textures, the selected text), and, while dropped and focused, a row
// of the three textures per item with the item's text and a highlight bar under the row the mouse is over. The
// device is clipped before the origin is set, as in the stock code
INT NCComboBox::OnPaint(NCGDevice* Device)
{
    guard(NCComboBox::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT TextYOfs = RowStep / 2 - RowSize.cy / 2;
    UTexture* Left;
    UTexture* Mid;
    UTexture* Right;
    DWORD Color;
    if (IsEnabled())
    {
        Left = LeftTex;
        Mid = MidTex;
        Right = RightTex;
        Color = 0xFFDCDCDC;
    }
    else
    {
        Left = LeftDisabledTex;
        Mid = MidDisabledTex;
        Right = RightDisabledTex;
        Color = 0xFF7E7E7E;
    }
    Device->DrawTexture(0, (INT)(Height * 0.5f - (FLOAT)(RowStep / 2)), 8, RowStep, 0, 0, 8, 17, Left, 0xFF, 1);
    Device->DrawTexture(8, (INT)(Height * 0.5f - (FLOAT)(RowStep / 2)), (INT)(Width - 16.0f), RowStep, 0, 0, 8, 17,
                        Mid, 0xFF, 1);
    Device->DrawTexture((INT)(Width - 8.0f), (INT)(Height * 0.5f - (FLOAT)(RowStep / 2)), 8, RowStep, 0, 0, 8, 17,
                        Right, 0xFF, 1);
    if (Items.Num() > Selected && Selected >= 0)
    {
        const TCHAR* Text = Items(Selected).GetText();
        Device->DrawText(TextX, ClosedH / 2 + TextY - RowStep / 2 + TextYOfs, Color, Text, 0, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0, 0);
    }
    if (bDropped && IsFocused())
    {
        for (INT i = 0; i < Items.Num(); i++)
        {
            Device->DrawTexture(0, RowStep * i + ClosedH, 8, RowStep, 0, 0, 8, 17, LeftTex, 0xFF, 1);
            Device->DrawTexture(8, RowStep * i + ClosedH, (INT)(Width - 16.0f), RowStep, 0, 0, 8, 17, MidTex, 0xFF, 1);
            Device->DrawTexture((INT)(Width - 8.0f), RowStep * i + ClosedH, 8, RowStep, 0, 0, 8, 17, RightTex, 0xFF,
                                1);
        }
        for (INT i = 0; i < Items.Num(); i++)
        {
            if (Hover == i)
            {
                Device->DrawStretchedTex(1, RowStep * i + ClosedH, (INT)(Width - 32.0f), 16, 0, 0, 16, 13, SelectTex,
                                         0xFF);
                Device->DrawStretchedTex((INT)(Width - 32.0f + 1.0f), RowStep * i + ClosedH, 32, 16, 0, 0, 32, 13,
                                         SelectTex2, 0xFF);
            }
            const TCHAR* Text = Items(i).GetText();
            Device->DrawText(TextX, RowStep * i + ClosedH + TextYOfs, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0,
                             0, 0, 0, 0, 0);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10005e40, NCComboBox, OnPaint)

// 0x10005bb0 (vslot 61) - the item under the mouse in the open list; the coordinates are 16-bit differences, as in
// the stock code. -1 outside the list part of the box
INT NCComboBox::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    WORD RX = (WORD)(LOWORD(lParam) - (WORD)ScreenX);
    WORD RY = (WORD)(HIWORD(lParam) - (WORD)ScreenY);
    if (RX != 0 && (FLOAT)RX < Width && (INT)RY > ClosedH && (FLOAT)RY < Height)
    {
        if (RowSize.cy == 0)
            Hover = 0;
        else
            Hover = ((INT)RY - ClosedH) / RowStep;
        if (Hover >= Items.Num())
            Hover = Items.Num() - 1;
        return 1;
    }
    Hover = -1;
    return 1;
}
NW_HOOK_METHOD(0x10005bb0, NCComboBox, OnMouseMove)

// 0x10005c60 (vslot 68) - open it, or pick the row under the mouse
INT NCComboBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (IsEnabled())
    {
        if (!bDropped)
        {
            DropDown();
            return 1;
        }
        if (Hover >= 0)
        {
            Selected = Hover;
            NotifyParent(Hover);
            bDropped = 0;
            DropDown();
        }
        KillFocus();
    }
    return 1;
}
NW_HOOK_METHOD(0x10005c60, NCComboBox, OnLButtonDown)

// 0x10006440 (vslot 69) - opens the list (when enabled) and sizes the window to it
INT NCComboBox::DropDown()
{
    if (IsEnabled())
    {
        if (!bDropped)
            bDropped = 1;
        UpdateSize();
    }
    return 1;
}
NW_HOOK_METHOD(0x10006440, NCComboBox, DropDown)

// 0x10005d10
void NCComboBox::SetSelected(INT Index)
{
    Selected = Index;
}
NW_HOOK_METHOD(0x10005d10, NCComboBox, SetSelected)

// 0x10005d20 - open: tall enough for the items, then the focus (vslot 29); closed: back to the closed size
void NCComboBox::UpdateSize()
{
    if (bDropped)
    {
        SetWindowSize((FLOAT)ClosedW, (FLOAT)(RowStep * Items.Num() + ClosedH));
        SetFocus();
        return;
    }
    SetWindowSize((FLOAT)ClosedW, (FLOAT)ClosedH);
}
NW_HOOK_METHOD(0x10005d20, NCComboBox, UpdateSize)

// 0x10006ac0 - the array goes, then NControl (OnDestroy has freed the texts)
NW_HOOK_DTOR(0x10006ac0, NCComboBox)

NW_CHECK_VTABLE(NCComboBox, 0x101a0090, 76)
