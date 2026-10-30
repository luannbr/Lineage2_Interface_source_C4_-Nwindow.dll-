/*=============================================================================
    NCMacroEditList.cpp - the lines of the macro editor: twelve edit boxes, nine shown, scrolled by a slider.
    Stock (C3): 0x1009B3E0 .. 0x1009B45F (the slider), 0x1009D730 .. 0x1009DB5F (the helpers, the scrolling,
    ValidateLines), 0x1009EE90 .. 0x1009F25F (OnCreate, OnPaint, OnDestroy), 0x100A0170 .. 0x100A030F (constructors,
    destructor).
=============================================================================*/
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NItemC4.h"

enum
{
    EDIT_FirstID = 0x1506,         // the line boxes' ids (name ours)
    EDIT_LineMaxLen = 32,          // the characters a line holds
    EDIT_LineX = 20,
    EDIT_LineY = 7,                // C2: 8
    EDIT_LineWidth = 202,          // C2: 205
    EDIT_LineHeight = 17,          // C2: 16
    EDIT_LinePitch = 26,           // px from a line to the next
    EDIT_ExtraWidth = 0x108,       // NCEditBox's fourth constructor argument
};

/*-----------------------------------------------------------------------------
    Constructors and destructor.
-----------------------------------------------------------------------------*/

// 0x100ac840 - the default constructor (the engine's InternalConstructor 0x100ad310 jumps here): the line boxes and the
// parser are constructed, the numbers cleared
NCMacroEditList::NCMacroEditList()
{
    VisibleLines = 0;
    TopLine = 0;
    TotalLines = 0;
}
NW_HOOK_CTOR(0x100ac840, NCMacroEditList)

// 0x100ac960 (ret 8; C3: two numbers, C2 one) - the lines shown and the range of the slider (NCMacroEditWnd makes it
// with 9 and 12)
NCMacroEditList::NCMacroEditList(INT InVisibleLines, INT InTotalLines)
{
    TopLine = 0;
    TotalLines = InTotalLines;
    VisibleLines = InVisibleLines;
}
NW_HOOK_CTOR_ARGS(0x100ac960, NCMacroEditList, (INT InVisibleLines, INT InTotalLines), (InVisibleLines, InTotalLines))

// 0x100ac8f0 - FParser's destructor for the parser, the pointer array's (0x1009f8f0) for the line boxes
NW_HOOK_DTOR(0x100ac8f0, NCMacroEditList)

// 0x10014b90 (vslot 25, ret 0xC): the stock body is NCItemWnd's (NCScrollWnd's and the result as 0 / 1), not the base class's
INT NCMacroEditList::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

/*-----------------------------------------------------------------------------
    The slider and the lines' positions.
-----------------------------------------------------------------------------*/

// 0x100a7a90 - the slider repeats every 200 ms; its range and its value
void NCMacroEditList::InitSlider()
{
    SetRepeatDelay(200);
    ScrollBar->SetRange(TotalLines, VisibleLines);
    ScrollBar->SetValue(TopLine);
}
NW_HOOK_METHOD(0x100a7a90, NCMacroEditList, InitSlider)

// 0x100a7ad0 - the range is the twelve lines; the first line shown is kept where the lines shown still fit
void NCMacroEditList::UpdateSlider()
{
    TotalLines = NMACRO_EditLines;
    if (VisibleLines <= NMACRO_EditLines)
    {
        if (NMACRO_EditLines - TopLine < VisibleLines)
            TopLine = NMACRO_EditLines - VisibleLines;
    }
    ScrollBar->SetRange(NMACRO_EditLines, VisibleLines);
    ScrollBar->SetValue(TopLine);
}
NW_HOOK_METHOD(0x100a7ad0, NCMacroEditList, UpdateSlider)

// 0x100a9e80 - the first line box that has the focus
INT NCMacroEditList::GetFocusLine()
{
    for (INT i = 0; i < Edits.Num(); i++)
    {
        NCEditBox* Edit = Edits(i);
        if (Edit != NULL && Edit->IsFocused())
            return i;
    }
    return -1;
}
NW_HOOK_METHOD(0x100a9e80, NCMacroEditList, GetFocusLine)

// 0x100a9ec0 (ret 4; C3: the new first line is an argument) - the boxes that leave the window are hidden, the ones that
// come in shown, then the nine from the new top are put under one another (20, 7 + 26 each) and shown. The first line
// shown is the new one at the end
void NCMacroEditList::LayoutLines(INT NewTopLine)
{
    if (NewTopLine == TopLine)
    {
        for (INT i = NewTopLine + NMACRO_ShownLines; i < NMACRO_EditLines; i++)
            Edits(i)->HideWindow();
    }
    else
    {
        if (TopLine < NewTopLine)
        {
            for (INT i = TopLine; i < NewTopLine; i++)
                Edits(i)->HideWindow();
        }
        else
        {
            for (INT i = NewTopLine; i < TopLine; i++)
                Edits(i)->ShowWindow();
            for (INT i = NewTopLine + NMACRO_ShownLines; i < TopLine + NMACRO_ShownLines; i++)
                Edits(i)->HideWindow();
        }
        TopLine = NewTopLine;
    }
    for (INT i = TopLine; i < TopLine + NMACRO_ShownLines; i++)
    {
        // the stock code fills four dwords from the window ({ ScreenX, ScreenY, Width, Height }); SetScreenPos reads x and y
        NRectF Origin = Rect;
        NCEditBox* Edit = Edits(i);
        Edit->SetScreenPos(EDIT_LineX, (i - TopLine) * EDIT_LinePitch + EDIT_LineY, (const POINT*)&Origin);
        Edit->ShowWindow();
    }
    TopLine = NewTopLine;
}
NW_HOOK_METHOD(0x100a9ec0, NCMacroEditList, LayoutLines)

// 0x100a9e20 - the line box under (X, Y): the window found there when it is an edit box, its number among the twelve
INT NCMacroEditList::HitLine(INT X, INT Y)
{
    NWndEntry* Found = FindWindowAt(X, Y);
    if (Found != NULL && Found->Wnd != NULL)
    {
        NCWnd* Wnd = Found->Wnd;
        if (Wnd->IsA(NCEditBox::StaticClass()))
        {
            for (INT i = 0; i < NMACRO_EditLines; i++)
            {
                if (Wnd == (NCWnd*)Edits(i))
                    return i;
            }
        }
    }
    return -1;
}
NW_HOOK_METHOD(0x100a9e20, NCMacroEditList, HitLine)

/*-----------------------------------------------------------------------------
    Scrolling (the slider's value is the first line shown).
-----------------------------------------------------------------------------*/

// 0x100aa000 (vslot 71, ret 4) - the slider moved: nothing to do when it is the line shown already
INT NCMacroEditList::OnScrollBar(INT Value)
{
    if (Value != TopLine)
    {
        LayoutLines(Value);
        UpdateSlider();
    }
    return 1;
}
NW_HOOK_METHOD(0x100aa000, NCMacroEditList, OnScrollBar)

// 0x100aa030 (vslot 75) - a line up, when there are lines to scroll
INT NCMacroEditList::ScrollUp()
{
    if (VisibleLines < TotalLines)
    {
        INT Line = TopLine - 1;
        if (Line < 0)
            Line = 0;
        ScrollBar->SetValue(Line);
        LayoutLines(Line);
        UpdateSlider();
    }
    return 1;
}
NW_HOOK_METHOD(0x100aa030, NCMacroEditList, ScrollUp)

// 0x100aa070 (vslot 76) - a line down, not past the last window of lines
INT NCMacroEditList::ScrollDown()
{
    if (VisibleLines < TotalLines)
    {
        INT Max = TotalLines - VisibleLines;
        INT Line = TopLine + 1;
        if (Line > Max)
            Line = Max;
        ScrollBar->SetValue(Line);
        LayoutLines(Line);
        UpdateSlider();
    }
    return 1;
}
NW_HOOK_METHOD(0x100aa070, NCMacroEditList, ScrollDown)

/*-----------------------------------------------------------------------------
    The window.
-----------------------------------------------------------------------------*/

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100ab500 (vslot 37) - the twelve line boxes (ids 0x1506.., 32 characters, 202 x 17 at 26 pixel steps from 7; the
// ones from VisibleLines on start hidden) chained in the tab order (the last one back to the window), then the slider
INT NCMacroEditList::OnCreate()
{
    NCScrollWnd::OnCreate();
    Edits.Empty();
    INT Index = 0;
    for (INT Y = EDIT_LineY; Y < 0x13F; Y += EDIT_LinePitch)
    {
        NCEditBox* Edit = new NCEditBox(0, 0, 1, EDIT_ExtraWidth, 0);
        Edit->SetMaxLength(EDIT_LineMaxLen);   // the box's longest text
        Edit->ID = Index + EDIT_FirstID;
        Edit->SetPosition(EDIT_LineX, Y, EDIT_LineWidth, EDIT_LineHeight);
        CreateChild(Edit, NWS_VISIBLE);
        if (Index >= VisibleLines)
            Edit->HideWindow();
        Edits.AddItem(Edit);
        Index++;
    }
    SetTabOrder(Edits(0)->Entry, Edits(11)->Entry);
    for (INT i = 0; i < NMACRO_EditLines; i++)
    {
        NWndEntry* Prev;
        NWndEntry* Next;
        if (i == 0)
        {
            Prev = Entry;
            Next = Edits(1)->Entry;
        }
        else if (i != 11)
        {
            Prev = Edits(i - 1)->Entry;
            Next = Edits(i + 1)->Entry;
        }
        else
        {
            Prev = Edits(10)->Entry;
            Next = Entry;
        }
        Edits(i)->SetTabOrder(Next, Prev);
    }
    InitSlider();
    return 1;
}
NW_HOOK_METHOD(0x100ab500, NCMacroEditList, OnCreate)

// 0x100ab690 (vslot 56) - the number of each line shown ("00".."11", in the big digits, grey), then, when a box has the
// focus outside the nine shown, the lines scroll to it
INT NCMacroEditList::OnPaint(NCGDevice* Device)
{
    guard(NCMacroEditList::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    for (INT i = TopLine; i < TopLine + NMACRO_ShownLines; i++)
    {
        TCHAR Text[68];
        appSprintf(Text, i < 10 ? TEXT("0%d") : TEXT("%d"), i);
        // C4: DrawSpecialDigit takes a sixth argument (the alpha, 0xFF)
        Device->DrawSpecialDigit(1, 2, (i - TopLine) * EDIT_LinePitch + 11, 0xFFA3A3A3, Text, 0xFF);
    }
    INT Focus = GetFocusLine();
    if (Focus >= 0)
    {
        INT Target = Focus - (NMACRO_ShownLines - 1);
        if (TopLine < Target || (Target = Focus, Focus < TopLine))
            OnScrollBar(Target);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ab690, NCMacroEditList, OnPaint)

// 0x100ab7f0 (vslot 38) - the line boxes go
INT NCMacroEditList::OnDestroy()
{
    guard(NCMacroEditList::OnDestroy);
    for (INT i = 0; i < Edits.Num(); i++)
    {
        if (Edits(i) != NULL)
        {
            RemoveChildWindow(Edits(i)->Entry);
            delete Edits(i);
            Edits(i) = NULL;
        }
    }
    Edits.Empty();
    NCScrollWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ab7f0, NCMacroEditList, OnDestroy)

/*-----------------------------------------------------------------------------
    The check of the lines before a macro is saved.
-----------------------------------------------------------------------------*/

// 0x100aa0c0 - the lines up to the first empty one hold what the parser takes as a command (no text after an empty line;
// the first line may not be empty). The commands that take numbers need them: /delay a positive one, /useshortcut (the
// three forms) a page 1..10 and a slot 1..12 whose shortcut is not itself a macro; the commands of the GMs need the
// player to be one. C4: any other line that is plain text (a code below 0x6E, or 0x20000000) ends the check with "valid"
// at once (the stock returns 1 there); a code that is none of them is refused.
UBOOL NCMacroEditList::ValidateLines()
{
    TCHAR Line[3068];   // (the stock buffer is 3072 characters; ours keeps a few more locals in the frame, so the sum of the frame matches at 3068)
    UBOOL bFilled = 1;
    for (INT i = 0; i < NMACRO_EditLines; i++)
    {
        const TCHAR* Text = Edits(i)->CommitText();
        if (Text == NULL)
        {
            if (bFilled)
            {
                if (i == 0)
                    return 0;
                bFilled = 0;
            }
            continue;
        }
        if (!bFilled)
            return 0;
        appStrcpy(Line, Text);
        INT Command = Parser.GetCommand(Line);
        if (Command == 0x4C)
        {
            // /delay <seconds>
            TCHAR* Token = Parser.GetNextToken();
            if (Token == NULL || NWtol(Token) < 1)
                return 0;
        }
        else if (Command < 0x4C && Command >= 0x1B && Command <= 0x1D)
        {
            // /useshortcut <page> <slot>
            TCHAR* Token = Parser.GetNextToken();
            if (Token == NULL)
                return 0;
            INT Page = NWtol(Token);
            if (Page < 1 || Page > 10)
                return 0;
            Token = Parser.GetNextToken();
            if (Token == NULL)
                return 0;
            INT Slot = NWtol(Token);
            if (Slot < 1 || Slot > 12)
                return 0;
            if (AsC4((NItemEntry*)&GConsole->GetDesktop()->ShortCutWnd->Items[(Slot + Page * 12) - 13])->Unknown1910
                == SHORTCUT_Macro)
                return 0;
        }
        else if (Command >= 0x10000000 && Command <= 0x10000001)
        {
            User* Player = GConsole->GetMyUser();
            if (Player == NULL || Player->BuilderLevel == 0)
                return 0;
        }
        else
        {
            if (Command < 0x6E || Command == 0x20000000)
                return 1;
            return 0;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100aa0c0, NCMacroEditList, ValidateLines)

NW_CHECK_VTABLE(NCMacroEditList, 0x101ba660, 78)
