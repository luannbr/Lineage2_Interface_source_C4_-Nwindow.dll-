/*=============================================================================
    L2TaskBox.cpp - the L2 skin's list of open conversations (C3 only; C3 addresses). Stock: OnPaint 0x10124780 ..
    0x1012498f, OnLButtonUp 0x10124990, AddString 0x10124eb0 .. 0x10124fbd, the constructors / destructor 0x10125fa0 ..
    0x101260bf. WindowProc, OnCreate and HitTest are TaskBox's (the same stock functions, hooked in TaskBox.cpp).
=============================================================================*/
#include "nwindow/L2TaskBox.h"
#include "nwindow/L2Task.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x1014aa70 - the base constructor, the three button textures none, 16 visible lines of at most 200
L2TaskBox::L2TaskBox()
{
    NormalTex = NULL;
    SelectedTex = NULL;
    AlertTex = NULL;
    MaxLines = 0xC8;
    VisibleLines = 0x10;
}
NW_HOOK_CTOR(0x1014aa70, L2TaskBox)

// 0x1014ab30 (ret 0xC) - the base constructor, the sizes given, the three button textures none (the third argument is never read)
L2TaskBox::L2TaskBox(INT InMaxLines, INT InVisibleLines, INT InUnknown20C)
{
    MaxLines = InMaxLines;
    NormalTex = NULL;
    SelectedTex = NULL;
    AlertTex = NULL;
    VisibleLines = InVisibleLines;
}
NW_HOOK_CTOR_ARGS(0x1014ab30, L2TaskBox, (INT A, INT B, INT C), (A, B, C))

// 0x1014aae0
NW_HOOK_DTOR(0x1014aae0, L2TaskBox)

// 0x10124390 (vslot 25) - TaskBox's body (hooked there): the list box's, then a press (0x201) goes to the click handler
INT L2TaskBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCListBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDOWN)
        return OnLButtonUp(wParam, lParam) != 0;
    return 0;
}

// 0x10124720 (vslot 37) - TaskBox's body (hooked there): the three button textures; the text at (4, 4)
INT L2TaskBox::OnCreate()
{
    NCListBox::OnCreate();
    NormalTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton"));
    SelectedTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton_down"));
    AlertTex = FindTexture(TEXT("L2UI_CH3.Msn.msnbutton_light"));
    TextX = 4;
    TextY = 4;
    return 1;
}

// 0x10119ea0 (vslot 69, ret 8) - TaskBox's body (hooked there): the lines are 21 pixels high from the top of the box
INT L2TaskBox::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0)
        return -1;
    INT W = (INT)Width;
    if (RX > W)
        return -1;
    if (RY < 0)
        return -1;
    if (RY > (INT)Height)
        return -1;
    INT Top = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i < NumLines; i++, Top += 21)
    {
        if (RX < W && RY >= Top && RY < Top + 21)
        {
            Selected = i;
            return Selected;
        }
    }
    return Selected;
}

// 0x10149250 (vslot 56, guard: L2TaskBox::OnPaint) - TaskBox's buttons (pressed / lit while its L2Task blinks / plain)
// with the name cut to 116 pixels at +3, and no icon
INT L2TaskBox::OnPaint(NCGDevice* Device)
{
    guard(L2TaskBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        L2Task* Entry = (L2Task*)Item.Param;
        UTexture* Tex;
        if (i == Selected)
            Tex = SelectedTex;
        else if (Entry == NULL || Entry->bAlert == 0)
            Tex = NormalTex;
        else if (Entry->bBlink == 0)
            Tex = NormalTex;
        else
            Tex = AlertTex;
        Device->DrawTexture(0, Line * 0x15, 0x8C, 0x15, 0, 0, 0x8C, 0x15, Tex, 0xFF, 1);
        TCHAR Text[256];
        NFitText(Item.Text->GetAt(0), Text, 0x74, 0xC);
        Device->DrawText(TextX + 3, TextY + Line * 0x15, Item.Text->Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10149250, L2TaskBox, OnPaint)

// 0x10149460 (vslot 69) - TaskBox's click on the L2Task fields (+0xD4 the window, +0xD8 the alert)
INT L2TaskBox::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT Hit = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected == Hit && Selected >= 0 && Selected < Items.Num())
    {
        L2Task* Entry = (L2Task*)Items(Selected).Param;
        if (Entry && Entry->ChatWindow)
        {
            NCWnd* Chat = Entry->ChatWindow;
            Entry->bAlert = 0;
            Chat->ShowWindow();
            Chat->SetFocus();
        }
    }
    Selected = -1;
    return 1;
}
NW_HOOK_METHOD(0x10149460, L2TaskBox, OnLButtonUp)

// 0x10149960 (vslot 70, ret 0xC, guard-less) - a line for Text (nothing for NULL): the Param is the second argument, the
// color the third; when the box is full the oldest line goes
void L2TaskBox::AddString(const TCHAR* Text, DWORD Param, INT Color)
{
    if (Text)
    {
        NString* Str = new NString(Text);
        Str->Color = Color;
        if (NumLines < MaxLines)
        {
            NumLines++;
            if (NumLines > VisibleLines)
                TopIndex++;
        }
        else
        {
            NString* Oldest = Items(0).Text;
            if (Oldest)
            {
                delete Oldest;
                Items(0).Text = NULL;
            }
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x101495B0, &Items, 0, 1);   // TListItemArray::Remove(0, 1)
#else
            Items.Remove(0, 1);                                  // TListItemArray::Remove(0, 1): the standalone compiles the template
#endif
        }
        INT i = Items.Add();
        Items(i).Text = Str;
        Items(i).Param = Param;
        Items(i).Unknown8 = 0;
    }
}
NW_HOOK_METHOD(0x10149960, L2TaskBox, AddString)

NW_CHECK_VTABLE(L2TaskBox, 0x101db378, 82)
