/*=============================================================================
    NCTextListBox.cpp - the text list of the C4 chat windows. Stock: 0x10021a20 / 0x10021ac0 (constructors), 0x10021a70
    (destructor), 0x10020890 (WindowProc), 0x100213c0 (OnCreate), 0x10021490 (OnDestroy), 0x100208b0 (ShowWindow), 0x10020bd0
    (OnPaint), 0x10021510 (AddString).
=============================================================================*/
#include "nwindow/NCTextListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NString.h"

// 0x10021a20 - NCListBox's constructor, the scroll controls on
NCTextListBox::NCTextListBox()
{
    bScrollControls = 1;
}
NW_HOOK_CTOR(0x10021a20, NCTextListBox)

// 0x10021ac0 (ret 0x10; anonymous in the db) - NCListBox's three-argument constructor, then the flag
NCTextListBox::NCTextListBox(INT InMaxLines, INT InVisibleLines, INT Unused, INT bInScrollControls)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
    bScrollControls = (BYTE)bInScrollControls;
}
NW_HOOK_CTOR_ARGS(0x10021ac0, NCTextListBox, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x10021a70
NW_HOOK_DTOR(0x10021a70, NCTextListBox)

// 0x10020890 (vslot 25) - the plain forward to the list box (no enabled check)
INT NCTextListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10020890, NCTextListBox, WindowProc)

// 0x100213c0 (vslot 37) - with the scroll controls: the two buttons and the bar go to the right edge (15 px wide, the bar between the
// buttons); the text starts at the corner
INT NCTextListBox::OnCreate()
{
    NCListBox::OnCreate();
    if (bScrollControls)
    {
        ScrollUpButton->SetPosition((INT)(Width - 15.0f), 0, 15, 15);
        ScrollDownButton->SetPosition((INT)(Width - 15.0f), (INT)(Height - 15.0f), 15, 15);
        ScrollBar->SetPosition((INT)(Width - 15.0f), 15, 15, (INT)(Height - 30.0f));
    }
    TextX = 0;
    TextY = 0;
    return 1;
}
NW_HOOK_METHOD(0x100213c0, NCTextListBox, OnCreate)

// 0x10021490 (vslot 38, guard)
INT NCTextListBox::OnDestroy()
{
    guard(NCTextListBox::OnDestroy);
    NCListBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10021490, NCTextListBox, OnDestroy)

// 0x100208b0 (vslot 48) - without the scroll controls the three are hidden again
void NCTextListBox::ShowWindow()
{
    NCListBox::ShowWindow();
    if (!bScrollControls)
    {
        ScrollUpButton->HideWindow();
        ScrollDownButton->HideWindow();
        ScrollBar->HideWindow();
    }
}
NW_HOOK_METHOD(0x100208b0, NCTextListBox, ShowWindow)

// 0x10020bd0 (vslot 62, guard) - the lines from TopIndex, one row of 17 px each (the text centered on the row by the height of an
// 'A'), at the indent of the line (the continuation lines of a wrapped text); the text starts 2 px in, the gap is 7
INT NCTextListBox::OnPaint(NCGDevice* Device)
{
    guard(NCTextListBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    LineGap = 7;
    TextX = 2;
    SIZE CharSize;
    NCGDevice::GetCharSize(&CharSize, 'A', 0);
    INT Row = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Row++)
    {
        NListItem Item = Items(i);
        NString* Line = Item.Text;
        INT Indent = Line->Indent;
        DWORD Color = Line->Color;
        Device->DrawText(TextX + Indent, TextY - CharSize.cy / 2 + Row * 17 + 9, Color, Line->GetAt(0), 0, 0, 0, L2FONT_Normal, 1,
                         0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10020bd0, NCTextListBox, OnPaint)

// One wrapped line at the end of the list (the stock repeats it inline for the first and for the next lines): the list keeps
// NumLines <= MaxLines and the view follows the last line; when it is full the oldest line is deleted. The element has a third
// field, cleared
static __forceinline void AppendWrappedLine(NCListBox* List, NString* Str, INT Param)
{
    if (List->NumLines < List->MaxLines)
    {
        if (++List->NumLines > List->VisibleLines)
            List->TopIndex++;
    }
    else
    {
        NString* Oldest = List->Items(0).Text;
        if (Oldest)
        {
            delete Oldest;
            List->Items(0).Text = NULL;
        }
        List->Items.Remove(0, 1);
    }
    INT i = List->Items.Add();
    List->Items(i).Text = Str;
    List->Items(i).Param = Param;
    List->Items(i).Unknown8 = 0;
    List->UpdateScrollBar();
}

// 0x10021510 (vslot 76) - the text is wrapped to the width less 22 pixels (the first line) and 34 (the next ones, indented 12); each
// line is a line of the list, in the color and with the Param
void NCTextListBox::AddString(const TCHAR* Text, DWORD Color, INT Param)
{
    if (Text)
    {
        const TCHAR* Part = NCGDevice::FirstWrappedLine(Text, (INT)(Width - 16.0f - 6.0f), 0);
        if (Part)
        {
            NString* Str = new NString(Part);
            Str->Color = Color;
            AppendWrappedLine(this, Str, Param);
            while ((Part = NCGDevice::NextWrappedLine((INT)(Width - 16.0f - 6.0f - 12.0f), 0)) != NULL)
            {
                Str = new NString(Part);
                Str->Color = Color;
                Str->Indent = 12;
                AppendWrappedLine(this, Str, Param);
            }
        }
    }
}
NW_HOOK_METHOD(0x10021510, NCTextListBox, AddString)

NW_CHECK_VTABLE(NCTextListBox, 0x101a5ab0, 82)
