/*=============================================================================
    L2ChatBox.cpp - the L2 skin's conversation text list (C3 only; C3 addresses). Stock: OnPaint 0x10124420 ..
    0x10124572, the constructors / destructor 0x101257d0 .. 0x1012586f. OnCreate (0x101243f0) is ChatBox's, the
    WindowProc NCDebugWnd's (0x101241c0).
=============================================================================*/
#include "nwindow/L2ChatBox.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// 0x1014a280 - the base constructor, then the vtable
L2ChatBox::L2ChatBox()
{
}
NW_HOOK_CTOR(0x1014a280, L2ChatBox)

// 0x1014a320 (ret 0xC) - the list box's three arguments
L2ChatBox::L2ChatBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
}
NW_HOOK_CTOR_ARGS(0x1014a320, L2ChatBox, (INT A, INT B, INT C), (A, B, C))

// 0x1014a2d0
NW_HOOK_DTOR(0x1014a2d0, L2ChatBox)

// 0x101241c0 (vslot 25) - the plain forward to the list box, the stock body is shared (hooked in NCChatListBox.cpp)
INT L2ChatBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x101243f0 (vslot 37) - ChatBox's body (hooked there): the text at (4, 4), the lines 18 apart
INT L2ChatBox::OnCreate()
{
    NCListBox::OnCreate();
    TextX = 4;
    TextY = 4;
    LineGap = 6;
    return 1;
}

// 0x10148f50 (vslot 56, guard: L2ChatBox::OnPaint) - the visible lines (12 + LineGap high), each wrapped 22 pixels short of
// the width; no icons
INT L2ChatBox::OnPaint(NCGDevice* Device)
{
    guard(L2ChatBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        Device->DrawText(TextX, (LineGap + 12) * Line + TextY, Item.Text->Color, Item.Text->GetAt(0),
                         (INT)(Width - 16.0f - 6.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10148f50, L2ChatBox, OnPaint)

// 0x10149630 (ret 0xC) - the first line of Text (the width less 16 + 6, and 12 for the indent) is copied (with three spaces in
// front when bIndent) into a string of its own, and each following line the same; the box keeps MaxLines of them (the oldest
// goes: out of line for the first, inline for the others)
void L2ChatBox::Add(const TCHAR* Text, DWORD Color, INT bIndent)
{
    TCHAR Buffer[256];
    FLOAT Indent = (FLOAT)(bIndent ? 12 : 0);
    TCHAR* Piece = NCGDevice::FirstWrappedLine(Text, (INT)(Width - 16.0f - 6.0f - Indent), 0);
    if (!Piece)
        return;
    if (bIndent)
        appSprintf(Buffer, TEXT("   %s"), Piece);
    else
        appStrcpy(Buffer, Piece);
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
    Items(i).Param = -1;
    Items(i).Unknown8 = 0;
    UpdateScrollBar();
    while ((Piece = NCGDevice::NextWrappedLine((INT)(Width - 16.0f - 6.0f - Indent), 0)) != NULL)
    {
        if (bIndent)
            appSprintf(Buffer, TEXT("   %s"), Piece);
        else
            appStrcpy(Buffer, Piece);
        Str = new NString(Buffer);
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
NW_HOOK_METHOD(0x10149630, L2ChatBox, Add)

// 0x10149b70 (ret 0xC) - with a name: SystemMsg 1326 (a template with "$s1") with the name put in (if the template has no "$s1"
// the name replaces it), added in grey, then the text in the usual color (0xFFDCDCDC; Color is not used) indented under it;
// without a name (or an empty one): the text alone in Color
void L2ChatBox::AddMessage(const TCHAR* From, const TCHAR* Text, DWORD Color)
{
    if (From && *From)
    {
        TCHAR Line[1024];
        FString Template(*GL2GameData.SystemMsg[1326].Message);
        FString Result = Template.Substitute(FString(TEXT("$s1")), FString(From));
        if (Result == Template)
            Result = From;
        appSprintf(Line, TEXT("%s"), *Result);
        Add(Line, 0xFF929292, 0);
        Add(Text, 0xFFDCDCDC, 1);
    }
    else
        Add(Text, Color, 0);
}
NW_HOOK_METHOD(0x10149b70, L2ChatBox, AddMessage)

NW_CHECK_VTABLE(L2ChatBox, 0x101daea0, 82)
