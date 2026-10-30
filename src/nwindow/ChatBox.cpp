/*=============================================================================
    ChatBox.cpp - the conversation's text list (C3 only; C3 addresses). Stock: OnPaint 0x1011a910 .. 0x1011ab3f, Add
    0x1011bb00 .. 0x1011bd4c, the constructors / destructor 0x1011da80 .. 0x1011db1f, and OnCreate 0x101243f0 (the body
    L2ChatBox shares). The WindowProc is NCDebugWnd's (0x101241c0).
=============================================================================*/
#include "nwindow/ChatBox.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x101425f0 - the base constructor, then the vtable
ChatBox::ChatBox()
{
}
NW_HOOK_CTOR(0x101425f0, ChatBox)

// 0x10142690 (ret 0xC) - the list box's three arguments
ChatBox::ChatBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
}
NW_HOOK_CTOR_ARGS(0x10142690, ChatBox, (INT A, INT B, INT C), (A, B, C))

// 0x10142640
NW_HOOK_DTOR(0x10142640, ChatBox)

// 0x101241c0 (vslot 25) - the plain forward to the list box, the stock body is shared (hooked in NCChatListBox.cpp)
INT ChatBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1013f170 (vslot 37) - the text at (4, 4), the lines 18 apart; the stock body is L2ChatBox's too
INT ChatBox::OnCreate()
{
    NCListBox::OnCreate();
    TextX = 4;
    TextY = 4;
    LineGap = 6;
    return 1;
}
NW_HOOK_METHOD(0x1013f170, ChatBox, OnCreate)

// 0x10140680 (ret 0xC, name ours) - Text is cut into lines that fit the box (the width less 22: 16 for the scroll bar and 6; less 37 with
// the flag, for the 15 pixels of indent a flagged line gets); each is added in Color (the box keeps MaxLines of them, the oldest goes) and what is left is worked on again.
// C4: no three spaces in front of the continuation lines any more; the flag (bIndent) is kept in the line's Param (0 or 1) and the paint
// moves a flagged line 15 pixels right. A blank line (the piece is empty) skips one character of what is left
void ChatBox::Add(const TCHAR* Text, DWORD Color, INT bIndent)
{
    TCHAR Buffer[2048];
    appSprintf(Buffer, TEXT("%s"), Text);
    INT Wrap = (INT)(Width - (bIndent ? 37.0f : 22.0f));
    TCHAR* Piece;
    while ((Piece = NCGDevice::FirstWrappedLine(Buffer, Wrap, 0)) != NULL)
    {
        NString* Str = new NString(Piece);
        Str->Color = Color;
        INT bFlag = (bIndent != 0);
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
        Items(i).Param = bFlag;
        Items(i).Unknown8 = 0;
        UpdateScrollBar();
        if (appStrcmp(Piece, TEXT("")) == 0)
            appSprintf(Buffer, TEXT("%s"), Buffer + 1);
        else
            appSprintf(Buffer, TEXT("%s"), Buffer + appStrlen(Piece));
    }
}
NW_HOOK_METHOD(0x10140680, ChatBox, Add)

// 0x101403f0 (ret 0xC, name ours) - the first line (35 pixels short of the width) gets the icon, the lines after it (22 short)
// none; the first line is removed from a full box through the out-of-line TListItemArray::Remove (0x1012ca10), the others
// inline
void ChatBox::AddWithIcon(INT Icon, const TCHAR* Text, DWORD Color)
{
    TCHAR* Line = NCGDevice::FirstWrappedLine(Text, (INT)(Width - 16.0f - 6.0f - 13.0f), 0);
    if (!Line)
        return;
    NString* Str = new NString(Line);
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
    Items(i).Param = -1;
    Items(i).Unknown8 = Icon;
    UpdateScrollBar();
    while ((Line = NCGDevice::NextWrappedLine((INT)(Width - 16.0f - 6.0f), 0)) != NULL)
    {
        Str = new NString(Line);
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
        i = Items.Add();
        Items(i).Text = Str;
        Items(i).Param = -1;
        Items(i).Unknown8 = 0;
        UpdateScrollBar();
    }
}
NW_HOOK_METHOD(0x101403f0, ChatBox, AddWithIcon)

// 0x10141220 (ret 0xC, name ours) - with a person: SystemMsg 1326 (a template with "$s1") with the person's decoded name
// put in (if the template has no "$s1" the name goes in front of it), added in grey, then the text in the usual color (0xFFDCDCDC; Color is not used)
// indented under it; without a person: the text alone in Color
void ChatBox::AddMessage(const TCHAR* Text, NChatUser* From, DWORD Color)
{
    if (From)
    {
        TCHAR Line[1024];
        FString Template(*GL2GameData.SystemMsg[1326].Message);
        FString Name = From->GetDecodedName();
        FString Result = Template.Substitute(FString(TEXT("$s1")), Name);
        if (Result == Template)
            Result = (Name += Template);
        appSprintf(Line, TEXT("%s"), *Result);
        Add(Line, 0xFF929292, 0);
        Add(Text, 0xFFDCDCDC, 1);
    }
    else
        Add(Text, Color, 0);
}
NW_HOOK_METHOD(0x10141220, ChatBox, AddMessage)

// 0x1013f1a0 (vslot 62) - the visible lines (12 + LineGap high). A line with an icon (15 x 15) has its text 17 pixels further and 2 lower,
// wrapped 35 pixels short of the width; one without starts at the text origin (the flagged one 15 pixels right of it) and wraps 22 short
INT ChatBox::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        if (Item.Unknown8)
        {
            Device->DrawTexture(TextX, (LineGap + 12) * Line + TextY, 0xF, 0xF, 0, 0, 0xF, 0xF, (UTexture*)Item.Unknown8, 0xFF, 1);
            Device->DrawText(TextX + 0x11, (LineGap + 12) * Line + 2 + TextY, Item.Text->Color, Item.Text->GetAt(0),
                             (INT)(Width - 16.0f - 6.0f - 13.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Item.Param == 0)
            Device->DrawText(TextX, (LineGap + 12) * Line + TextY, Item.Text->Color, Item.Text->GetAt(0),
                             (INT)(Width - 16.0f - 6.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else
            Device->DrawText(TextX + 0xF, (LineGap + 12) * Line + TextY, Item.Text->Color, Item.Text->GetAt(0),
                             (INT)(Width - 16.0f - 6.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x1013f1a0, ChatBox, OnPaint)

NW_CHECK_VTABLE(ChatBox, 0x101d9790, 82)
