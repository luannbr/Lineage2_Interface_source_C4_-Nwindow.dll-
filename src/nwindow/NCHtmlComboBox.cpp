/*=============================================================================
    NCHtmlComboBox.cpp - the combo box of the HTML pages. Stock: 0x10028960 .. 0x100289ff,
    0x1002fcf0 .. 0x1002fe43, 0x10030230 .. 0x100305e5, 0x10033300 .. 0x10033538 (ShowWindow
    and HideWindow are bodies the linker folded with other classes).
=============================================================================*/
#include "nwindow/NCHtmlComboBox.h"
#include "nwindow/NCHtmlButton.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// ';'-separated tokens of a list (the stock helper, name ours): *Token = the next one, cut out
// of Str in place; returns where the following one starts
static TCHAR* NextListToken(TCHAR* Str, TCHAR** Token)
{
    while (*Str == ';')
        Str++;
    if (*Str)
    {
        *Token = Str;
        while (*Str && *Str != ';')
            Str++;
        if (*Str)
        {
            *Str = 0;
            Str++;
        }
    }
    return Str;
}
NW_HOOK_FUNC(0x100471b0, NextListToken)

// 0x1003ebc0 - NCComboBox's, nothing more
NCHtmlComboBox::NCHtmlComboBox()
{
}
NW_HOOK_CTOR(0x1003ebc0, NCHtmlComboBox)

// 0x1003ec10
NW_HOOK_DTOR(0x1003ec10, NCHtmlComboBox)


// 0x10047180 (vslot 25) - NCComboBox's, as a boolean
INT NCHtmlComboBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCComboBox::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10047180, NCHtmlComboBox, WindowProc)

// 0x1004b280 (vslot 37) - NCComboBox::OnCreate with the HTML button for the drop button (1 pixel from the top, 15 x 15)
// and the "inputbox" textures
INT NCHtmlComboBox::OnCreate()
{
    NControl::OnCreate();
    LeftTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox1"));
    MidTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox2"));
    RightTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox3"));
    LeftDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox1_disable"));
    MidDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox2_disable"));
    RightDisabledTex = FindTexture(TEXT("L2UI_CH3.Etc.inputbox3_disable"));
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
    HtmlDropButton = new NCHtmlButton(TEXT("L2UI_CH3.Button.downbutton"), TEXT("L2UI_CH3.Button.downbutton_down"));
    HtmlDropButton->ID = NID_COMBO_DROP;
    HtmlDropButton->SetPosition((INT)(Rect.W - 15.0f - 1.0f), 1, 15, 15);
    CreateChild(HtmlDropButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1004b280, NCHtmlComboBox, OnCreate)

// 0x10047640 (vslot 38) - the drop button, then the items (NCComboBox's cleanup 0x100066D0: each record's string, the array
// emptied; NControl's OnDestroy follows, NCComboBox's is not called)
INT NCHtmlComboBox::OnDestroy()
{
    guard(NCHtmlComboBox::OnDestroy);
    if (HtmlDropButton)
    {
        RemoveChildWindow(HtmlDropButton->Entry);
        delete HtmlDropButton;
        HtmlDropButton = NULL;
    }
    FreeItems();   // 0x100066D0
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10047640, NCHtmlComboBox, OnDestroy)

// stock body shared with NCEditBox's (0x100a5d70)
void NCHtmlComboBox::ShowWindow()
{
    NCWnd::ShowWindow();
}

// stock body shared with NCChatWnd's (0x1003dab0)
void NCHtmlComboBox::HideWindow()
{
    NCWnd::HideWindow();
}

// the text of an item of the list (C4: the list is an array of 12-byte records, a string and a system string id; the stock
// function 0x10005B60 answers with the text of either)
static FORCEINLINE const TCHAR* ComboItemText(NCHtmlComboBox* Combo, INT Index)
{
    return Combo->Items(Index).GetText();
}

// 0x10047700 (vslot 62) - NCComboBox::OnPaint's drawing (the db names it NCComboBox: so does the guard string), clipped
// to the enclosing viewer while closed (to itself while open, so the list shows past the viewer's edge; no clip
// without a viewer), the origin set first. Always 17 pixel rows from the top of the box: the open list is the closed
// box once more per item, below it. C3: disabled, it draws the disabled textures (C2 drew the normal ones)
INT NCHtmlComboBox::OnPaint(NCGDevice* Device)
{
    guard(NCComboBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    if (bDropped)
        Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    else
    {
        NCWnd* Viewer = EnclosingHtmlViewer(this);
        if (Viewer)
            Device->SetClip(Viewer->ScreenX, Viewer->ScreenY, (INT)Viewer->Width, (INT)Viewer->Height);
    }
    INT TextYOfs = 9 - RowSize.cy / 2;
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
    Device->DrawTexture(0, 0, 8, 17, 0, 0, 8, 17, Left, 0xFF, 1);
    Device->DrawTexture(8, 0, (INT)(Width - 16.0f), 17, 0, 0, 8, 17, Mid, 0xFF, 1);
    Device->DrawTexture((INT)(Width - 8.0f), 0, 8, 17, 0, 0, 8, 17, Right, 0xFF, 1);
    if (Items.Num() > Selected && Selected >= 0)
        Device->DrawText(TextX, TextY + TextYOfs, Color, ComboItemText(this, Selected), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);
    if (bDropped && IsFocused())
    {
        for (INT i = 0; i < Items.Num() + 1; i++)
        {
            Device->DrawTexture(0, 17 * i, 8, 17, 0, 0, 8, 17, LeftTex, 0xFF, 1);
            Device->DrawTexture(8, 17 * i, (INT)(Width - 16.0f), 17, 0, 0, 8, 17, MidTex, 0xFF, 1);
            Device->DrawTexture((INT)(Width - 8.0f), 17 * i, 8, 17, 0, 0, 8, 17, RightTex, 0xFF, 1);
        }
        for (INT i = 0; i < Items.Num(); i++)
        {
            if (Hover == i)
            {
                Device->DrawStretchedTex(1, 17 * i + 17, (INT)(Width - 32.0f), 16, 0, 0, 16, 13, SelectTex, 0xFF);
                Device->DrawStretchedTex((INT)(Width - 32.0f + 1.0f), 17 * i + 17, 32, 16, 0, 0, 32, 13, SelectTex2,
                                         0xFF);
            }
            Device->DrawText(TextX, 17 * i + TextYOfs + 17, 0xFFDCDCDC, ComboItemText(this, i), 0, 0, 0, L2FONT_Normal, 0,
                             0, 0, 0, 0, 0, 0);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10047700, NCHtmlComboBox, OnPaint)

// 0x100476f0 (vslot 74) - the chosen item
const TCHAR* NCHtmlComboBox::GetValue()
{
    return GetItemText(Selected);
}
NW_HOOK_METHOD(0x100476f0, NCHtmlComboBox, GetValue)

// 0x10047200 (name ours) - the items from a ';'-separated List, the one equal to Sel selected (the last one when Sel
// is empty: the first). Sel must not be NULL (the stock code reads it after the loop)
INT NCHtmlComboBox::SetList(TCHAR* List, TCHAR* Sel)
{
    if (List && *List)
    {
        TCHAR* Token = NULL;
        INT Index = 0;
        TCHAR* Rest = NextListToken(List, &Token);
        while (Token && *Rest)
        {
            AddItem((const TCHAR*)Token);   // NCComboBox::AddItem(const TCHAR*), 0x100068b0
            if (Sel && !appStrcmp(Token, Sel))
                SetSelected(Index);
            Rest = NextListToken(Rest, &Token);
            Index++;
        }
        AddItem((const TCHAR*)Token);
        if (*Sel)
        {
            if (!appStrcmp(Token, Sel))
                SetSelected(Index);
        }
        else
            SetSelected(0);
    }
    return 1;
}
NW_HOOK_METHOD(0x10047200, NCHtmlComboBox, SetList)

NW_CHECK_VTABLE(NCHtmlComboBox, 0x101a88e8, 76)
