/*=============================================================================
    InviteBox.cpp - the friends' list of the invite window (C3 only; C3 addresses). Stock: 0x10119a10 .. 0x10119a86
    (WindowProc, the double click), 0x1011a260 .. 0x1011a4ff (OnCreate, OnPaint) and the constructors / destructor
    0x1011d500 .. 0x1011d5ff.
=============================================================================*/
#include "nwindow/InviteBox.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x10142070 - the base constructor, then 16 visible lines of at most 256
InviteBox::InviteBox()
{
    MaxLines = 0x100;
    VisibleLines = 0x10;
}
NW_HOOK_CTOR(0x10142070, InviteBox)

// 0x10142120 (ret 0xC) - the base constructor, then the two sizes (the third argument is never read)
InviteBox::InviteBox(INT InMaxLines, INT InVisibleLines, INT InUnknown20C)
{
    MaxLines = InMaxLines;
    VisibleLines = InVisibleLines;
}
NW_HOOK_CTOR_ARGS(0x10142120, InviteBox, (INT A, INT B, INT C), (A, B, C))

// 0x101420d0
NW_HOOK_DTOR(0x101420d0, InviteBox)

// 0x1013e2b0 (vslot 25) - the list box's, then the double click (0x203) goes to its handler
INT InviteBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCListBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1013e2b0, InviteBox, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1013eac0 (vslot 37) - the two highlight textures; the text starts at (4, 4)
INT InviteBox::OnCreate()
{
    NCListBox::OnCreate();
    HighlightTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    TextX = 4;
    TextY = 4;
    return 1;
}
NW_HOOK_METHOD(0x1013eac0, InviteBox, OnCreate)

// 0x1013ffc0 (ret 0x10, name ours) - the friend's line (nothing for NULL text): Text as it is when it fits the box (with room
// for the icon), else with "..." after it (the stock asks for the first wrapped line but formats the whole text); the person
// is the line's Param and the status Icon its extra field; a full list drops its oldest line through the out-of-line
// TListItemArray::Remove (0x1012ca10)
void InviteBox::AddFriend(UTexture* Icon, const TCHAR* Text, NChatUser* User, DWORD Color)
{
    if (Text)
    {
        TCHAR Buffer[260];
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        if (Size.cx + 0xF < (INT)Width - 0x16)
            appStrcpy(Buffer, Text);
        else
        {
            NCGDevice::FirstWrappedLine(Text, (INT)(Width - 16.0f - 15.0f - 30.0f), 0);
            appSprintf(Buffer, TEXT("%s..."), Text);
        }
        NString* Str = new NString(Buffer);
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
        Items(i).Param = (INT)User;
        Items(i).Unknown8 = (INT)Icon;
        UpdateScrollBar();
    }
}
NW_HOOK_METHOD(0x1013ffc0, InviteBox, AddFriend)

// 0x1013eb10 (vslot 56, guard: InviteBox::OnPaint) - the visible lines (12 + LineGap high): under the selected one the
// two parts of the highlight bar (235 and 32 wide), the line's icon (15 x 15) and its text cut to 250 pixels
INT InviteBox::OnPaint(NCGDevice* Device)
{
    guard(InviteBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        TCHAR Text[256];
        if (i == Selected)
        {
            Device->DrawStretchedTex(TextX, (LineGap + 12) * Line + TextY, 0xEB, 0xF, 0, 0, 0x10, 0xD, HighlightTex, 0xFF);
            Device->DrawStretchedTex(TextX + 0xEB, (LineGap + 12) * Line + TextY, 0x20, 0xF, 0, 0, 0x20, 0xD, SelectTex2, 0xFF);
        }
        if (Item.Unknown8)
            Device->DrawTexture(TextX, (LineGap + 12) * Line + TextY, 0xF, 0xF, 0, 0, 0xF, 0xF, (UTexture*)Item.Unknown8, 0xFF, 1);
        NFitText(Item.Text->GetAt(0), Text, 0xFA, 0xC);
        Device->DrawText(TextX + 0x12, (LineGap + 12) * Line + TextY, Item.Text->Color, Text, 0, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013eb10, InviteBox, OnPaint)

// 0x1013e310 (vslot 72, ret 8) - the double click is the parent's business
INT InviteBox::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    return SendToParent(WM_LBUTTONDBLCLK, wParam, lParam);
}
NW_HOOK_METHOD(0x1013e310, InviteBox, OnLButtonDblClick)

NW_CHECK_VTABLE(InviteBox, 0x101d93a0, 82)
