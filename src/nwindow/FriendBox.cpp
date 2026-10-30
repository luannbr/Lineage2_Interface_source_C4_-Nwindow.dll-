/*=============================================================================
    FriendBox.cpp - the roster list of a conversation (C3 only; C3 addresses). Stock: 0x1011a6f0 .. 0x1011a8ff
    (OnCreate, OnPaint) and 0x1011d9b0 .. 0x1011da7f (constructors, destructor). The WindowProc is NCDebugWnd's
    (0x101241c0, shared with the other lists, hooked by NCChatListBox.cpp).
=============================================================================*/
#include "nwindow/FriendBox.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x10142520 - the base constructor, then the vtable
FriendBox::FriendBox()
{
}
NW_HOOK_CTOR(0x10142520, FriendBox)

// 0x101425c0 (ret 0xC) - the list box's three arguments
FriendBox::FriendBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
}
NW_HOOK_CTOR_ARGS(0x101425c0, FriendBox, (INT A, INT B, INT C), (A, B, C))

// 0x10142570
NW_HOOK_DTOR(0x10142570, FriendBox)

// 0x101241c0 (vslot 25) - the stock body is the plain forward shared with NCDebugWnd's, hooked in NCChatListBox.cpp
INT FriendBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1013ef50 (vslot 37) - the text starts at (4, 2)
INT FriendBox::OnCreate()
{
    NCListBox::OnCreate();
    TextX = 4;
    TextY = 2;
    return 1;
}
NW_HOOK_METHOD(0x1013ef50, FriendBox, OnCreate)

// 0x10140180 (ret 0x48, name ours) - a line for the person: the decoded name in Color, the icon of the status (NLN online;
// BRB, IDL, AWY, LUN away; BSY, PHN busy; FLN offline, the fourth; any other status none); a full list drops its oldest line (the removal is
// inline here); the person comes by value and is destroyed at the end
void FriendBox::AddUser(NChatUser User, DWORD Color)
{
    UTexture* Icon = NULL;
    NString* Str = new NString(*User.GetDecodedName());
    if (User.Status == TEXT("NLN"))
        Icon = FindTexture(TEXT("L2UI_CH3.Msn.msnicon1"));
    else if (User.Status == TEXT("BRB") || User.Status == TEXT("IDL") || User.Status == TEXT("AWY") ||
             User.Status == TEXT("LUN"))
        Icon = FindTexture(TEXT("L2UI_CH3.Msn.msnicon2"));
    else if (User.Status == TEXT("BSY") || User.Status == TEXT("PHN"))
        Icon = FindTexture(TEXT("L2UI_CH3.Msn.msnicon3"));
    else if (User.Status == TEXT("FLN"))
        Icon = FindTexture(TEXT("L2UI_CH3.Msn.msnicon4"));
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
        NRemoveFirstLine(this);
    }
    INT i = Items.Add();
    Items(i).Text = Str;
    Items(i).Param = -1;
    Items(i).Unknown8 = (INT)Icon;
}
typedef void (FriendBox::*SigAddUser)(NChatUser, DWORD);
// installed as the member itself (no forwarder): the by-value person a forwarder would copy again is the stock's own
NW_HOOK_VARARGS(0x10140180, FriendBox, AddUser, SigAddUser)

// 0x1013ef80 (vslot 56, guard: FriendBox::OnPaint) - the origin and the gap are set again at every paint (4, 0 and 2);
// each visible line has its 15 x 15 icon and its text at +17 (one pixel below) cut to 201 pixels
INT FriendBox::OnPaint(NCGDevice* Device)
{
    guard(FriendBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    TextX = 4;
    TextY = 0;
    LineGap = 2;
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        if (Item.Unknown8)
            Device->DrawTexture(TextX, (LineGap + 12) * Line + TextY, 0xF, 0xF, 0, 0, 0xF, 0xF, (UTexture*)Item.Unknown8, 0xFF, 1);
        TCHAR Text[256];
        NFitText(Item.Text->GetAt(0), Text, 0xC9, 0xC);
        Device->DrawText(TextX + 0x11, (LineGap + 12) * Line + 2 + TextY, Item.Text->Color, Text, 0, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013ef80, FriendBox, OnPaint)

NW_CHECK_VTABLE(FriendBox, 0x101d9648, 82)
