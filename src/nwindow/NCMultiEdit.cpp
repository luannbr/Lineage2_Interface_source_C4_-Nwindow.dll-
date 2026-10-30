/*=============================================================================
    NCMultiEdit.cpp - the multi-line text box, its line list and string array, and the HTML
    pages' version of it. Stock: 0x1001a440 .. 0x10020c9d, 0x10021f00 .. 0x10022147 (NCMultiEdit),
    0x1002ff20 .. 0x10030154, 0x10030ff0 .. 0x100326cf, 0x10033540 .. 0x100338c2
    (NCHtmlMultiEdit). SetFocus and the HTML box's OnLButtonDown are bodies the linker folded with
    other classes'. The getters of the line count (0x10033ac0, hooked as NActionItem's) and of
    FString::Len (0x1001b380) are folded bodies other classes call too.
=============================================================================*/
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NCChineseIME.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCHtmlButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCHtmlSliderBar.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCCompositeBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NString.h"

#include <string.h>

enum { TIMER_MultiEditCaret = 0xFA0D05 };

// the code a click sends the viewer with the box's y (as NCHtmlEdit's): MAKELONG(100, LocalY)
enum { HTML_SCROLLTO = 100 };

// the composition window of the Chinese IME and its last composition
static FORCEINLINE NCCompositeBox* ChineseCompositeBox(NCIMEManager* Manager)
{
    return ((NCChineseIME*)Manager->ChineseIME)->CompositeBox;
}
static FORCEINLINE FString& ChineseLastComp(NCIMEManager* Manager)
{
    return ((NCChineseIME*)Manager->ChineseIME)->LastComp;
}

// the IME types whose composition is drawn in its own window (Chinese layouts)
static FORCEINLINE UBOOL IsWindowedIME(INT Type)
{
    return Type == 10 || Type == 9 || Type == 11 || Type == 12;
}

/*-----------------------------------------------------------------------------
    NLineNode, NLineList.
-----------------------------------------------------------------------------*/

// 0x10025290 (not inlined: the stock callers call it)
__declspec(noinline) NLineNode::NLineNode()
{
    Prev = NULL;
    Next = NULL;
    Text.Empty();
}
NW_HOOK_CTOR(0x10025290, NLineNode)

// 0x100252e0 (not inlined: the stock callers call it)
__declspec(noinline) NLineNode::~NLineNode()
{
    Prev = NULL;
    Next = NULL;
    Text.Empty();
}
NW_HOOK_DTOR(0x100252e0, NLineNode)

// 0x10025ad0 - one empty line
NLineList::NLineList()
{
    Count = 1;
    Unknown4 = 0;
    NLineNode* Node = new NLineNode;
    Head = Node;
    Tail = Node;
    Current = Node;
    CurIndex = 0;
}
NW_HOOK_CTOR(0x10025ad0, NLineList)

// 0x10029f30 - Count is cleared before Clear(), which then frees nothing: the nodes are left
// (sic; OnDestroy has emptied the list before)
NLineList::~NLineList()
{
    Count = 0;
    Unknown4 = 0;
    Clear();
    if (Head)
        delete Head;
}
NW_HOOK_DTOR(0x10029f30, NLineList)

// 0x10025bc0 - the node of a line, walked to from the nearest of the head, the tail and the cursor (which it
// moves). It walks the whole distance; an index out of range gives NULL
NLineNode* NLineList::GetLine(INT Index)
{
    if (Index >= Count)
        return NULL;
    if (Index == CurIndex)
        return Current;
    NLineNode* Node;
    if (Index < CurIndex)
    {
        INT Distance = CurIndex - Index;
        if (Distance < Index)
        {
            Node = Current;
            for (INT i = Distance; i > 0; i--)
                Node = StepBack();
            return Node;
        }
        Current = Head;
        CurIndex = 0;
        Node = Current;
        for (INT i = Index; i > 0; i--)
            Node = StepForward();
        return Node;
    }
    INT Distance = Index - CurIndex;
    INT FromTail = Count - Index - 1;
    if (FromTail < Distance)
    {
        Node = Tail;
        Current = Tail;
        CurIndex = Count - 1;
        for (INT i = 0; i < FromTail; i++)
            Node = StepBack();
        return Node;
    }
    Node = Current;
    for (INT i = Distance; i > 0; i--)
        Node = StepForward();
    return Node;
}
NW_HOOK_METHOD(0x10025bc0, NLineList, GetLine)

// 0x10025b50 - the characters of every line (the cursor ends on the last one)
INT NLineList::TotalLength()
{
    INT Total = 0;
    Current = Head;
    CurIndex = 0;
    for (INT i = 0; i < Count; i++)
    {
        if (!Current)
            break;
        Total += Current->Text.Len();
        if (!StepForward())
            break;
    }
    return Total;
}
NW_HOOK_METHOD(0x10025b50, NLineList, TotalLength)

// 0x10027da0 - -1 appends; an index out of range frees the new node and fails
INT NLineList::Insert(INT Index)
{
    NLineNode* Node = new NLineNode;
    if (Index == -1)
    {
        if (Count)
        {
            if (GetLine(Count - 1) && Node)
            {
                GetLine(Count - 1)->Next = Node;
                Node->Prev = GetLine(Count - 1);
                Count++;
                Tail = Node;
                return 1;
            }
        }
        else if (Node)
        {
            if (Head)
                delete Head;
            Head = Node;
            Current = Node;
            CurIndex = 0;
            Tail = Node;
        }
        Count++;
        return 1;
    }
    if (Index > Count || Index < 0)
    {
        if (Node)
            delete Node;
        return 0;
    }
    NLineNode* At = NULL;
    if (Index < Count && GetLine(Index))
        At = GetLine(Index);
    if (Index > 0 && GetLine(Index - 1))
    {
        NLineNode* Before = GetLine(Index - 1);
        if (Before)
        {
            Before->Next = Node;
            Node->Prev = Before;
        }
    }
    if (Node)
        Node->Next = At;
    if (At)
        At->Prev = Node;
    if (Index == 0)
    {
        Head = Node;
        if (Count == 0)
        {
            Current = Node;
            Tail = Node;
            CurIndex = 0;
        }
    }
    if (Index == Count)
        Tail = Node;
    if (Index <= CurIndex && Count)
        CurIndex++;
    Count++;
    return 1;
}
NW_HOOK_METHOD(0x10027da0, NLineList, Insert)

// 0x10027f40 - the line's text is emptied, the node unlinked and freed; the cursor stays on its line
void NLineList::RemoveAt(INT Index)
{
    if (Index < Count && Index > -1 && GetLine(Index))
    {
        NLineNode* Node = GetLine(Index);
        GetLine(Index)->Text.Empty();
        if (Node)
        {
            if (Index < CurIndex && CurIndex > 0)
                CurIndex--;
            else if (Index == CurIndex)
            {
                if (Node->Prev)
                {
                    Current = Node->Prev;
                    CurIndex--;
                }
                else
                    Current = Node->Next;
            }
            if (Node->Prev)
                Node->Prev->Next = Node->Next;
            if (Node->Next)
                Node->Next->Prev = Node->Prev;
            if (Index == 0)
                Head = Node->Next;
            if (Index == Count - 1)
                Tail = Node->Prev;
            delete Node;
            Count--;
        }
    }
}
NW_HOOK_METHOD(0x10027f40, NLineList, RemoveAt)

// 0x10028000 - every line removed, as many times as there were lines
void NLineList::Clear()
{
    for (INT i = Count; i > 0; i--)
        RemoveAt(0);
    Head = NULL;
    Current = NULL;
    Tail = NULL;
    CurIndex = 0;
}
NW_HOOK_METHOD(0x10028000, NLineList, Clear)

/*-----------------------------------------------------------------------------
    NStringArray: C3 has no such array. The C2 box held one (Strings, 0xC bytes, constructed with the
    box and counted by NCHtmlMultiEdit's scrolling); the C3 constructors build no second list before
    Lines, and the scrolling reads TotalRows.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    NCMultiEdit.
-----------------------------------------------------------------------------*/

// 0x1002a040 - NControl's constructor is inlined. C3: the box no longer holds an NStringArray (the C2
// constructor built one before Lines)
NCMultiEdit::NCMultiEdit()
{
    WhiteTex = NULL;
    TextBuf = NULL;
    CaretTimer = 0;
    CaretLine = 0;
    CaretCol = 0;
    bCaretOn = 0;
    VisibleRows = 0;
    TopLine = 0;
    NumLines = 1;
    CaretRow = 0;
    CaretX = 0;
    bCompRow = 0;
    Unknown220 = 0;
    UpButton = NULL;
    DownButton = NULL;
    ScrollBar = NULL;
    bReading = 0;
    Unknown214 = 0;
    Unknown216 = 0;
    Unknown254 = 0;
    Unknown258 = 0;
    IMECaretX = 0;
    IMECaretY = 0;
    TotalRows = 1;
    Unknown250 = 1;
    Unknown24C = 1;
    Unknown25C = 16;
}
NW_HOOK_CTOR(0x1002a040, NCMultiEdit)

// 0x10029fa0 - the implicit destructor: Reading, Comp, IntList (TIntArray's, 0x100231D0), Lines
NW_HOOK_DTOR(0x10029fa0, NCMultiEdit)



// 0x10024040 (vslot 25) - the scroll controls, then the IME's messages
INT NCMultiEdit::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MULTIEDIT_UP)
            return OnScrollUp() != 0;
        if (wParam == (WPARAM)NID_MULTIEDIT_DOWN)
            return OnScrollDown() != 0;
        if (wParam == (WPARAM)NID_MULTIEDIT_BAR)
            return OnScrollBar(lParam) != 0;
        return 0;
    }
    if (Msg == (UINT)NIM_COMPOSITION)
        return OnComposition(wParam, lParam) != 0;
    if (Msg == (UINT)NIM_RESULT)
        return OnResult(wParam, lParam) != 0;
    if (Msg == (UINT)NIM_GETCARET)
        return OnGetCaret(wParam, lParam) != 0;
    if (Msg == (UINT)NIM_READING)
        return OnReading(wParam, lParam) != 0;
    if (Msg == (UINT)NIM_ATTRIBUTE)
        return OnAttribute(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10024040, NCMultiEdit, WindowProc)

// 0x10024190 - the caret's row stays among the visible ones, and the page does not run past the last row
// (C3: the second test is new)
void NCMultiEdit::ScrollToCaret()
{
    if (TopLine + VisibleRows <= CaretRow)
        TopLine = CaretRow - VisibleRows + 1;
    if (TotalRows < VisibleRows + TopLine && TopLine > 0)
        TopLine = TotalRows - VisibleRows;
    if (CaretRow < TopLine)
        TopLine = CaretRow;
}
NW_HOOK_METHOD(0x10024190, NCMultiEdit, ScrollToCaret)

// 0x100241f0 (vslot 29) - the focus, then the IME manager child is told (a tail jump to its slot 69)
void NCMultiEdit::SetFocus()
{
    NCWnd::SetFocus();
    IMEManager->Activate();
}
NW_HOOK_METHOD(0x100241f0, NCMultiEdit, SetFocus)

// 0x10024210 (vslot 48) - shown, then the scroll bar follows the text (a tail jump to its own slot 69)
void NCMultiEdit::ShowWindow()
{
    NCWnd::ShowWindow();
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x10024210, NCMultiEdit, ShowWindow)

// 0x10024230 (vslot 30)
void NCMultiEdit::KillFocus()
{
    IMEManager->Deactivate();
    NCWnd::KillFocus();
}
NW_HOOK_METHOD(0x10024230, NCMultiEdit, KillFocus)

// 0x10024250 (vslot 36) - blinks the caret, whatever the timer
INT NCMultiEdit::OnTimer(INT TimerID)
{
    bCaretOn = !bCaretOn;
    return 1;
}
NW_HOOK_METHOD(0x10024250, NCMultiEdit, OnTimer)

// 0x10025050 (vslot 81) - answers with the caret position the painters left
INT NCMultiEdit::OnGetCaret(WPARAM wParam, LPARAM lParam)
{
    if (wParam)
        SendTo((NWndEntry*)wParam, NIM_CARET, IMECaretX, IMECaretY);
    return 1;
}
NW_HOOK_METHOD(0x10025050, NCMultiEdit, OnGetCaret)

// 0x10025080 (vslot 82)
INT NCMultiEdit::OnReading(WPARAM wParam, LPARAM lParam)
{
    bReading = 1;
    Reading.Empty();
    TCHAR* Str = IMEManager->GetReadingStr();
    if (Str)
    {
        ReadingParam = lParam;
        Reading = Str;
    }
    return 1;
}
NW_HOOK_METHOD(0x10025080, NCMultiEdit, OnReading)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100250d0 (vslot 83)
INT NCMultiEdit::OnAttribute(WPARAM wParam, LPARAM lParam)
{
    bReading = 0;
    Reading.Empty();
    return 1;
}
NW_HOOK_METHOD(0x100250d0, NCMultiEdit, OnAttribute)

// 0x10025230 (vslot 78) - the scroll bar was dragged
INT NCMultiEdit::OnScrollBar(INT Value)
{
    TopLine = Value;
    return 1;
}
NW_HOOK_METHOD(0x10025230, NCMultiEdit, OnScrollBar)

// 0x10024e60 - the composing characters, on white while the caret blinks on
void NCMultiEdit::PaintCompositionKR(FString Text, INT X, INT Y, NCGDevice* Device)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    if (bCaretOn)
    {
        Device->DrawStretchedTex(LineGap + X, Y, Size.cx, 12, WhiteTex);
        Device->DrawText(LineGap + X, Y, 0xFF000000, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0, 0);
    }
    else
        Device->DrawText(LineGap + X, Y, 0xFFDCDCDC, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                         0, 0);
}
NW_HOOK_METHOD(0x10024e60, NCMultiEdit, PaintCompositionKR)

// 0x10024270 - NCEditBox's Japanese painters (PaintCompositionJP, PaintComposingJP) in one, for the row's part of
// the composition (Text, from CompStart; the characters drawn come from Comp): the lines by attribute along the
// clauses (from the clause the row starts in), then the dotted underline of the characters still being typed. The
// first target character of the composition (*Out still 0) gives the IME its caret, below the row (RowNum).
void NCMultiEdit::PaintCompositionJP(FString Text, INT CompStart, INT X, INT Y, INT RowNum, NCGDevice* Device, INT* Out)
{
    SIZE Size;
    INT LineY = Y + 13;
    NCIME* IME = IMEManager->JapaneseIME;
    INT Offset = 0;
    UBOOL bTarget = 0;
    FString Ch;
    INT ClauseIdx = 0;
    for (INT c = 0; c < IME->CompClauseLen; c++)
    {
        if (CompStart < (INT)IME->CompClause[c])
        {
            ClauseIdx = c - 1;
            break;
        }
    }
    DWORD* Clause = &IME->CompClause[ClauseIdx + 1];
    for (INT i = 0; i < Text.Len(); i++)
    {
        BYTE* Attr = &IME->CompAttr[i + CompStart];
        if ((*Attr & 1) && !bTarget)
        {
            bTarget = 1;
            if (!*Out)
            {
                IMECaretX = LineGap + Offset + X;
                IMECaretY = RowNum * 15 + 2;
                *Out = 1;
            }
        }
        INT CharX = LineGap + Offset + X;
        INT CharW = Device->DrawText(CharX, Y, 0xFFDCDCDC, *Comp.Mid(i + CompStart, 1), 0, 0, 0, L2FONT_Normal,
                                     0, 0, 0, 0, 0, 0);
        Offset += CharW;
        INT EndX = LineGap + Offset + X;
        if (*Clause == (DWORD)(i + CompStart + 1))
        {
            Clause++;
            EndX--;
        }
        if (IsFocused())
        {
            if ((*Attr & 1) && CharW > 0)
            {
                Device->DrawLine(CharX, LineY, EndX, Y + 14, 0xFFDCDCDC, 0);
                Device->DrawLine(CharX, LineY, EndX, LineY, 0xFFDCDCDC, 0);
            }
            if ((*Attr & 3) && CharW > 0)
                Device->DrawLine(CharX, LineY, EndX, LineY, 0xFFDCDCDC, 0);
            if (*Attr & 2)
                Device->DrawLine(CharX, LineY, EndX, LineY, 0xFFDCDCDC, 0);
            if (*Attr & 4)
                Device->DrawLine(CharX, LineY, EndX, LineY, 0xFFFF0000, 0);
        }
    }
    INT UnderlineW = 0;
    INT StartX = 0;
    INT Pos = CompStart;
    for (INT j = 0; j < Text.Len(); j++)
    {
        if (IMEManager->JapaneseIME->CompAttr[Pos] == ATTR_INPUT)
        {
            if (j == 0 || IMEManager->JapaneseIME->CompAttr[Pos - 1] != ATTR_INPUT)
            {
                Ch = Comp.Mid(CompStart, j);
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                StartX = Size.cx;
                Ch = Comp.Mid(Pos, 1);
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                UnderlineW += Size.cx;
            }
            else
            {
                Ch = Comp.Mid(Pos, 1);
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                UnderlineW += Size.cx;
            }
        }
        Pos++;
    }
    if (UnderlineW > 0)
    {
        for (INT k = 0; k < UnderlineW; k += 3)
            Device->DrawText(StartX + X + LineGap + k, Y + 3, 0xFFDCDCDC, TEXT("."), 0, 0, 0, L2FONT_Normal, 0, 0,
                             0, 0, 0, 0);
    }
}
NW_HOOK_METHOD(0x10024270, NCMultiEdit, PaintCompositionJP)

// 0x10024740 - NCEditBox's Taiwanese painters in one, for the row's part of the composition (Text, from
// CompStart), with the clause the row starts in: the target clause black on gray while focused, bit 2 underlined in
// red, and for the IME types 5 and 7 the other characters in red; a dotted underline for the other types. The first
// target character gives the IME its caret (*Out still 0; the Y is negative, the Taiwanese IME turns it); only when
// that character is 0 px wide does the reading string show there (as NCEditBox's ClauseW): a column of 12 px
// cells below the row for the type 6 (error position on in red on gray, the rest in blue), in the line black on
// gray for the type 4.
void NCMultiEdit::PaintCompositionTW(FString Text, INT CompStart, INT X, INT Y, INT RowNum, NCGDevice* Device, INT* Out)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Reading, 0, 0, 0, 0, 0, L2FONT_Normal);
    DWORD ReadingW = Size.cx;
    if (ReadingW > 0 && GConsole->GetIMEType() == 6)
        ReadingW = 12;
    NCIME* IME = IMEManager->TaiwanIME;
    INT Offset = 0;
    INT ClauseX = 0;
    UBOOL bTarget = 0;
    UBOOL bClauseX = 0;
    INT ClauseIdx = 0;
    for (INT c = 0; c < IME->CompClauseLen; c++)
    {
        if (CompStart < (INT)IME->CompClause[c])
        {
            ClauseIdx = c - 1;
            break;
        }
    }
    DWORD* Clause = &IME->CompClause[ClauseIdx + 1];
    for (INT i = 0; i < Text.Len(); i++)
    {
        INT CharX = LineGap + Offset + X;
        INT CharW = Device->DrawText(CharX, Y, 0xFFDCDCDC, *Text.Mid(i, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                                     0);
        BYTE* Attr = &IME->CompAttr[i + CompStart];
        if ((*Attr & 1) && !bTarget)
        {
            bTarget = 1;
            if (!*Out)
            {
                IMECaretX = LineGap + Offset + X;
                IMECaretY = -2 - RowNum * 15;
                *Out = 1;
            }
            if (!CharW)
            {
                ClauseX = Offset;
                bClauseX = 1;
            }
        }
        Offset += CharW;
        INT EndX = LineGap + Offset + X;
        if (*Clause == (DWORD)(i + CompStart + 1))
        {
            Clause++;
            EndX--;
        }
        if (IsFocused())
        {
            INT Marks = 0;
            if ((*Attr & 1) && CharW > 0)
            {
                Device->DrawStretchedTex(CharX, Y, EndX - CharX, 12, FindTexture(TEXT("NWindow.GrayTexture")));
                Device->DrawText(CharX, Y, 0xFF000000, *Text.Mid(i, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                Marks = 1;
            }
            if (*Attr & 4)
            {
                Device->DrawLine(CharX, Y + 13, EndX, Y + 13, 0xFFFF0000, 0);
                Marks++;
            }
            if (!Marks && (GConsole->GetIMEType() == 5 || GConsole->GetIMEType() == 7))
                Device->DrawText(CharX, Y, 0xFFFF0000, *Text.Mid(i, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
    }
    if (Text.Len() > 0 && GConsole->GetIMEType() != 5 && GConsole->GetIMEType() != 7)
    {
        NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0, L2FONT_Normal);
        for (INT k = 0; k < Size.cx; k += 3)
            Device->DrawText(X + k, Y + 3, 0xFFDCDCDC, TEXT("."), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    if (bReading && bClauseX)
    {
        INT RX = LineGap + ClauseX + X;
        if (GConsole->GetIMEType() == 6)
        {
            Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)(Height + 100.0f));
            INT RY = Y + 6;
            Device->DrawStretchedTex(RX, RY, 12, 48, FindTexture(TEXT("NWindow.WhiteTexture")));
            for (INT k = 0; k < Reading.Len(); k++)
            {
                if (k >= ReadingParam)
                {
                    Device->DrawStretchedTex(RX, RY, 12, 12, FindTexture(TEXT("NWindow.GrayTexture")));
                    Device->DrawText(RX, RY, 0xFFFF0000, *Reading.Mid(k, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                                     0, 0);
                }
                else
                    Device->DrawText(RX, RY, 0xFF0000FF, *Reading.Mid(k, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                                     0, 0);
                RY += 12;
            }
            Device->ReleaseClip();
        }
        else if (GConsole->GetIMEType() == 4)
        {
            Device->DrawStretchedTex(RX, Y, ReadingW, 12, FindTexture(TEXT("NWindow.GrayTexture")));
            Device->DrawText(RX, Y, 0xFF000000, *Reading, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
    }
}
NW_HOOK_METHOD(0x10024740, NCMultiEdit, PaintCompositionTW)

// 0x10025330 - the Chinese IME's part (nothing composed hides the composition window). The IME type 8 draws the
// row's part of the composition with a dotted underline; the first target character (or typed one: that test
// reads the attribute at the row's index, not the composition's, as in the stock code) goes black on white with the
// IME's caret 5 px past its start and 15 px down, where the composition window shows the reading string (hidden
// without one) and the candidate box goes; the caret follows the row's part when the whole composition has
// neither. The types 9 to 12 put the composition window at the row's start 15 px down (9 and 10 fill it with the
// row's part), the candidate box 105 px right of the row's start, and blink the caret at the row's start. Only the
// type 10 shows the window's caret.
void NCMultiEdit::PaintCompositionCN(FString Text, INT CompStart, INT X, INT Y, INT RowNum, NCGDevice* Device, INT* Out)
{
    if (IMEManager->IsNotComposing())
    {
        ChineseCompositeBox(IMEManager)->HideWindow();
        return;
    }
    NCChineseIME* IME = (NCChineseIME*)IMEManager->ChineseIME;
    NCCandidateBox* Cand = IME->CandidateBox;
    FLOAT CandW = 0;
    FLOAT CandH = 0;
    if (Cand)
    {
        CandH = Cand->Height;
        CandW = Cand->Width;
    }
    NCCompositeBox* Box = IME->CompositeBox;
    if (GConsole->GetIMEType() == 10 || GConsole->GetIMEType() == 9)
    {
        IMECaretX = LineGap + X + 105;
        IMECaretY = Y + 15;
        Box->SetText(*Text);
        Box->SetPosition(LineGap + X, IMECaretY, 100, 15);
        Box->ShowWindow();
        if (Cand)
            Cand->SetPosition(IMECaretX, IMECaretY, (INT)CandW, (INT)CandH);
        if (IsFocused() && bCaretOn)
            Device->DrawText(LineGap + X - 2, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    else if (GConsole->GetIMEType() == 11 || GConsole->GetIMEType() == 12)
    {
        IMECaretX = LineGap + X + 105;
        IMECaretY = Y + 15;
        Box->SetPosition(LineGap + X, Y + 15, 100, 15);
        Box->ShowWindow();
        if (Cand)
            Cand->SetPosition(IMECaretX, IMECaretY, (INT)CandW, (INT)CandH);
        if (IsFocused() && bCaretOn)
            Device->DrawText(LineGap + X - 2, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    else if (GConsole->GetIMEType() == 8)
    {
        SIZE Size;
        FString Ch;
        Device->DrawText(LineGap + X, Y, 0xFFDCDCDC, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        INT Offset = 0;
        NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0, L2FONT_Normal);
        INT TextW = Size.cx;
        for (INT k = 0; k < TextW; k += 3)
            Device->DrawText(LineGap + k - 2 + X, Y + 3, 0xFFDCDCDC, TEXT("."), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                             0, 0);
        INT i;
        for (i = 0; i < Text.Len(); i++)
        {
            Ch = Text.Mid(i, 1);
            NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
            NCIME* CN = IMEManager->ChineseIME;
            if (CN->CompAttr[i + CompStart] == ATTR_TARGET_CONVERTED || CN->CompAttr[i] == ATTR_INPUT)
            {
                IMECaretX = LineGap + Offset + 5 + X;
                IMECaretY = Y + 15;
                break;
            }
            Offset += Size.cx;
        }
        Ch = Text.Mid(i, 1);
        NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
        INT CharW = Size.cx;
        if (IsFocused() && bCaretOn && i == Comp.Len() - CompStart)
        {
            INT j;
            for (j = 0; j < Comp.Len(); j++)
            {
                BYTE Attr = IMEManager->ChineseIME->CompAttr[j];
                if (Attr == ATTR_TARGET_CONVERTED || Attr == ATTR_INPUT)
                    break;
            }
            if (j == Comp.Len())
                Device->DrawText(LineGap + TextW + X - 2, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0,
                                 0, 0, 0, 0);
        }
        Device->DrawStretchedTex(LineGap + Offset + X, Y, CharW, 12, FindTexture(TEXT("NWindow.WhiteTexture")));
        Device->DrawText(LineGap + Offset + X, Y, 0xFF000000, *Text.Mid(i, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
        Box->SetText(*Reading);
        Box->SetPosition(IMECaretX, IMECaretY, 50, 15);
        if (Reading.Len())
            Box->ShowWindow();
        else
            Box->HideWindow();
        if (Cand)
            Cand->SetPosition(IMECaretX, IMECaretY, (INT)CandW, (INT)CandH);
    }
    if (GConsole->GetIMEType() == 10)
        Box->SetShowCaret(1);
    else
        Box->SetShowCaret(0);
}
NW_HOOK_METHOD(0x10025330, NCMultiEdit, PaintCompositionCN)

// 0x100250f0 (vslot 73) - rows per notch; scrolling down is bounded by the number of rows
INT NCMultiEdit::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Delta = (SWORD)((SWORD)HIWORD(wParam) / 120);
    if (Delta > 0)
    {
        if (TopLine > Delta)
            TopLine -= Delta;
        else
            TopLine = 0;
    }
    else
    {
        INT Visible = VisibleRows;
        INT D = Delta;
        if (TotalRows >= Visible)
        {
            if (TotalRows + (D - TopLine) > Visible)
                TopLine -= D;
            else
                TopLine = TotalRows - Visible;
        }
        else if (TotalRows + (D - TopLine) > Visible)
            TopLine -= D;
        else
            TopLine = 0;
    }
    ScrollBar->SetValue(TopLine);
    return 1;
}
NW_HOOK_METHOD(0x100250f0, NCMultiEdit, OnMouseWheel)

// 0x100251a0 (vslot 76)
INT NCMultiEdit::OnScrollUp()
{
    if (TotalRows > VisibleRows)
    {
        if (--TopLine < 0)
            TopLine = 0;
        ScrollBar->SetValue(TopLine);
    }
    return 1;
}
NW_HOOK_METHOD(0x100251a0, NCMultiEdit, OnScrollUp)

// 0x100251e0 (vslot 77)
INT NCMultiEdit::OnScrollDown()
{
    INT Visible = VisibleRows;
    if (TotalRows > Visible)
    {
        TopLine++;
        if (TopLine > TotalRows - Visible)
            TopLine = TotalRows - Visible;
        ScrollBar->SetValue(TopLine);
    }
    return 1;
}
NW_HOOK_METHOD(0x100251e0, NCMultiEdit, OnScrollDown)

// 0x10025a60 - repeat period of both scroll buttons
void NCMultiEdit::SetRepeatDelay(INT Delay)
{
    if (UpButton && DownButton)
    {
        UpButton->RepeatDelay = Delay;
        DownButton->RepeatDelay = Delay;
    }
}
NW_HOOK_METHOD(0x10025a60, NCMultiEdit, SetRepeatDelay)

// 0x10025a90 - the range is given the scroll position for a total (sic)
void NCMultiEdit::InitScrollBar()
{
    SetRepeatDelay(200);
    ScrollBar->SetRange(TopLine, VisibleRows);
    ScrollBar->SetValue(TopLine);
}
NW_HOOK_METHOD(0x10025a90, NCMultiEdit, InitScrollBar)

// 0x10025250 (vslot 75) - the bar counts rows (C2: lines)
void NCMultiEdit::UpdateScrollBar()
{
    ScrollBar->SetRange(TotalRows, VisibleRows);
    ScrollBar->SetValue(TopLine);
}
NW_HOOK_METHOD(0x10025250, NCMultiEdit, UpdateScrollBar)

// 0x10025db0 - screen rows of a line; with bComp the composition is counted at the caret
INT NCMultiEdit::CountRows(INT Line, UBOOL bComp)
{
    if (Line >= NumLines)
        return 0;
    INT X = 0;
    INT Rows = 1;
    FString Text(Lines.GetLine(Line)->Text);
    FString Ch;
    SIZE Size;
    for (INT i = 0; i <= Text.Len(); i++)
    {
        if (bComp && Line == CaretLine && i == CaretCol)
        {
            for (INT k = 0; k < Comp.Len(); k++)
            {
                Ch = Comp.Mid(k, 1);
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                X += Size.cx;
                if (X > (INT)Width - 36)
                {
                    Rows++;
                    X = Size.cx;
                }
            }
        }
        if (i == Text.Len())
            break;
        Ch = Text.Mid(i, 1);
        NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
        X += Size.cx;
        if (X > (INT)Width - 36)
        {
            Rows++;
            X = Size.cx;
        }
    }
    return Rows;
}
NW_HOOK_METHOD(0x10025db0, NCMultiEdit, CountRows)

// 0x10025ff0 - adds the rows a line gained since OldRows to TotalRows (it never takes any away here)
void NCMultiEdit::UpdateRowCount(INT Line, INT OldRows, UBOOL bComp)
{
    INT Rows = CountRows(Line, bComp);
    if (OldRows < Rows)
        TotalRows += Rows - OldRows;
}
NW_HOOK_METHOD(0x10025ff0, NCMultiEdit, UpdateRowCount)

// 0x10024f90 (name ours) - the length of a line without its line break (0 for an empty line). C4: only a '\n' counts as a
// break, taken together with the '\r' before it ("\r\n"); a lone '\r' stays in the line
INT NCMultiEdit::LineLength(FString Line)
{
    if (Line.Num() != 0 && Line.Num() - 1 > 0)
    {
        INT Length = Line.Num() - 1;
        if ((*Line)[Length - 1] == '\n')
        {
            if (Length > 1 && (*Line)[Length - 2] == '\r')
                return Length - 2;
            Length--;
        }
        return Length;
    }
    return 0;
}
NW_HOOK_METHOD(0x10024f90, NCMultiEdit, LineLength)

// 0x10026020 - counts every screen row again and puts the caret on its row. A character that does not fit is
// measured again on the next row. A line ending in '\n' adds a row (C4: no longer '\r'). With Param set the caret is not
// moved once its line has wrapped before it. The size of a line that cannot be fetched is the last one measured.
// C2: the columns of a line run to its length without the line break, inclusive.
void NCMultiEdit::RecalcRows(INT Param)
{
    INT Wraps = 0;
    TotalRows = 1;
    UBOOL bCaretPlaced = 0;
    SIZE Size;
    Size.cx = 0;
    Size.cy = 0;
    for (INT Line = 0; Line < Lines.Num(); Line++)
    {
        INT X = 0;
        INT LineLen = LineLength(Lines.GetLine(Line)->Text);
        INT Col = 0;
        if (LineLen >= 0)
        {
            do
            {
                if (Line == CaretLine && Col == CaretCol)
                {
                    if (!bCaretPlaced && (!Param || !Wraps))
                    {
                        CaretX = X;
                        CaretRow = TotalRows - 1;
                    }
                    if (IMEManager->IsNotComposing() == 0)
                    {
                        if (!bCaretPlaced)
                        {
                            // the IME's cursor inside the composition gets the caret
                            NCIME* IME = NULL;
                            if (IMEManager)
                            {
                                if (GLanguageType == 2)
                                    IME = IMEManager->JapaneseIME;
                                else if (GLanguageType == 0)
                                    IME = IMEManager->KoreanIME;
                                else if (GLanguageType == 3)
                                    IME = IMEManager->TaiwanIME;
                            }
                            for (INT k = 0; k < Comp.Len() + 1; k++)
                            {
                                if (IME && IME->CursorPos == k)
                                {
                                    CaretX = X;
                                    CaretRow = TotalRows - 1;
                                }
                                if (GLanguageType != 4 || GConsole->GetIMEType() == 8)
                                {
                                    NCGDevice::GetTextExtent(&Size, *Comp.Mid(k, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                                    X += Size.cx;
                                    if (X > (INT)Width - 36)
                                    {
                                        X = 0;
                                        TotalRows++;
                                        k--;
                                    }
                                }
                            }
                        }
                        if (Lines.GetLine(Line))
                            NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                                     L2FONT_Normal);
                        X += Size.cx;
                        if (X > (INT)Width - 36)
                        {
                            bCaretPlaced = 1;
                            X = 0;
                            TotalRows++;
                            Col--;
                        }
                    }
                    else
                    {
                        if (Lines.GetLine(Line))
                            NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                                     L2FONT_Normal);
                        X += Size.cx;
                        if (X > (INT)Width - 36)
                        {
                            X = 0;
                            Col--;
                            Wraps++;
                            TotalRows++;
                        }
                    }
                }
                else
                {
                    if (Lines.GetLine(Line))
                        NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                                 L2FONT_Normal);
                    X += Size.cx;
                    if (X > (INT)Width - 36)
                    {
                        X = 0;
                        TotalRows++;
                        Col--;
                    }
                }
                Col++;
            } while (Col <= LineLen);
        }
        if (Lines.GetLine(Line))
        {
            INT Num = Lines.GetLine(Line)->Text.Num();
            if (Num != 0 && Num - 1 > 0)
            {
                INT Last = Lines.GetLine(Line)->Text.Len() - 1;
                if (Lines.GetLine(Line)->Text.Mid(Last, 1) == TEXT("\n"))
                    TotalRows++;
            }
        }
    }
    ScrollToCaret();
}
NW_HOOK_METHOD(0x10026020, NCMultiEdit, RecalcRows)

// 0x100264f0 - a result string goes into the line at the caret (the composition keeps what is left of it); a
// composition string only replaces Comp (C3: the scroll bar is brought up to date at the end)
void NCMultiEdit::InsertString(TCHAR* Str, UBOOL bResult)
{
    if (bResult)
    {
        if (Str && Lines.GetLine(CaretLine))
        {
            INT OldRows = CountRows(CaretLine, 1);
            FString Line(Lines.GetLine(CaretLine)->Text);
            INT Length = wcslen(Str);
            FString S(Str);
            FString L = Line.Left(CaretCol);
            FString R = Line.Right(Line.Len() - CaretCol);
            Lines.GetLine(CaretLine)->Text = L + S + R;
            CaretCol += Length;
            for (INT i = 0; i < S.Len(); i++)
            {
                SIZE Size;
                NCGDevice::GetTextExtent(&Size, *S.Mid(i, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                CaretX += Size.cx;
                if (CaretX > (INT)Width - 36)
                {
                    CaretRow++;
                    CaretX = Size.cx;
                }
            }
            if (Comp.Len() > appStrlen(Str))
                Comp = Comp.Right(Comp.Len() - appStrlen(Str));
            else
                Comp.Empty();
            UpdateRowCount(CaretLine, OldRows, 1);
        }
    }
    else if (Str)
    {
        INT OldRows = CountRows(CaretLine, 1);
        Comp = FString(Str);
        UpdateRowCount(CaretLine, OldRows, 1);
    }
    ScrollToCaret();
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x100264f0, NCMultiEdit, InsertString)

// the scroll bar of the box, the stock way: the object is allocated as the class does it and the three-texture
// constructor (0x1002C5D0, NCSliderBar.cpp) runs on it
static NCSliderBar* NewSliderBar(const TCHAR* TopTex, const TCHAR* MidTex, const TCHAR* BottomTex)
{
    return new NCSliderBar(TopTex, MidTex, BottomTex);
}

// 0x10026830 (vslot 37) - NControl::OnCreate is not called. C3: the text height is measured on the two bytes
// of a Korean "ga" (0xB0 0xA1), the scroll controls are the L2UI_CH3 ones, 15 x 15 px and inside the box's
// right edge (positions from the local rectangle, no longer outside it)
INT NCMultiEdit::OnCreate()
{
    WhiteTex = FindTexture(TEXT("NWindow.WhiteTexture"));
    IMEManager = new NCIMEManager(0);
    CreateChild(IMEManager, NWS_VISIBLE);
    CaretTimer = SetTimer(TIMER_MultiEditCaret, 500, 0);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, TEXT("\xAC00"), 0, 0, 0, 0, 0, L2FONT_Normal);   // sa-emu: the stock pushes L"\xAC00" (Hangul GA, 0x101A60D4, at 0x100268DE); this was the CP949 bytes read as Latin-1
    if (GLanguageType == 5)
        CharH = 12;
    else
        CharH = Size.cy;
    LineGap = 2;
    CharW = Size.cx;
    CharH2 = Size.cy;
    NumLines = 1;
    VisibleRows = (INT)(Height - 2.0f) / (CharH + 2);
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);

    UpButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"));
    UpButton->ID = NID_MULTIEDIT_UP;
    UpButton->SetPosition((INT)(Local.W - 15.0f), 0, 15, 15);
    CreateChild(UpButton, NWS_VISIBLE);

    DownButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"));
    DownButton->ID = NID_MULTIEDIT_DOWN;
    DownButton->SetPosition((INT)(Local.W - 15.0f), (INT)(Local.H - 15.0f), 15, 15);
    CreateChild(DownButton, NWS_VISIBLE);

    ScrollBar = NewSliderBar(TEXT("L2UI_CH3.ScrollBar.SliderBarTop"), TEXT("L2UI_CH3.ScrollBar.SliderBarCenter"),
                             TEXT("L2UI_CH3.ScrollBar.SliderBarBottom"));
    ScrollBar->ID = NID_MULTIEDIT_BAR;
    ScrollBar->SetPosition((INT)(Local.W - 15.0f), 15, 15, (INT)Local.H >= 30 ? (INT)(Local.H - 30.0f) : 0);
    CreateChild(ScrollBar, NWS_VISIBLE);
    ScrollBar->DoNothing(0);
    InitScrollBar();
    return 1;
}
NW_HOOK_METHOD(0x10026830, NCMultiEdit, OnCreate)

// 0x10026f30 - x of a caret column, the line wrapped as it is drawn; a column past the end leaves CaretX alone
void NCMultiEdit::SetCaretX(INT Line, INT Col)
{
    if (Line < NumLines)
    {
        FString Text;
        FString Ch;
        if (Lines.GetLine(Line))
            Text = Lines.GetLine(Line)->Text;
        INT X = 0;
        if (Col <= Text.Len())
        {
            for (INT i = 0; i < Col; i++)
            {
                SIZE Size;
                Ch = Text.Mid(i, 1);
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                X += Size.cx;
                if (X > (INT)Width - 36)
                    X = Size.cx;
            }
            CaretX = X;
        }
    }
}
NW_HOOK_METHOD(0x10026f30, NCMultiEdit, SetCaretX)

// 0x10027080 - column of its line where a screen row starts (-1 past the text); bCompRow is set when the row
// starts inside the composition, the result is then an index into Comp
INT NCMultiEdit::GetRowStart(INT Row)
{
    INT CurRow = 0;
    if (Row >= TotalRows || Lines.Num() <= 0)
    {
        bCompRow = 0;
        return -1;
    }
    INT Line = 0;
    do
    {
        if (CurRow == Row)
        {
            if (Line == CaretLine && CaretCol == 0 && !IMEManager->IsNotComposing())
            {
                bCompRow = 1;
                return 0;
            }
            bCompRow = 0;
            return 0;
        }
        INT X = 0;
        INT LineLen = 0;
        if (Lines.GetLine(Line))
            LineLen = Lines.GetLine(Line)->Text.Len() + 1;
        for (INT Col = 0; Col < LineLen; Col++)
        {
            SIZE Size;
            if (Line == CaretLine && Col == CaretCol && !IMEManager->IsNotComposing() &&
                !IsWindowedIME(GConsole->GetIMEType()))
            {
                for (INT k = 0; k < Comp.Len(); k++)
                {
                    NCGDevice::GetTextExtent(&Size, *Comp.Mid(k, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                    X += Size.cx;
                    if (X > (INT)Width - 36)
                    {
                        X = 0;
                        if (++CurRow == Row)
                        {
                            bCompRow = 1;
                            return k;
                        }
                        k--;
                    }
                }
            }
            if (Lines.GetLine(Line))
                NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                         L2FONT_Normal);
            else
                Size.cx = 0;
            X += Size.cx;
            if (X > (INT)Width - 36)
            {
                X = 0;
                if (++CurRow == Row)
                {
                    bCompRow = 0;
                    return Col;
                }
                Col--;
            }
        }
        CurRow++;
        Line++;
    } while (Line < Lines.Num());
    bCompRow = 0;
    return -1;
}
NW_HOOK_METHOD(0x10027080, NCMultiEdit, GetRowStart)

// 0x10027420 - column where the row starting at Start of Line ends (0: at the end of the line, -1 past the text).
// A row that starts inside the composition (bCompRow) measures the rest of it first; one that breaks inside
// the composition returns an index into Comp with bCompRow set. The line is fetched without a NULL test.
INT NCMultiEdit::GetRowEnd(INT Start, INT Line)
{
    INT X = 0;
    if (Line >= Lines.Num())
    {
        bCompRow = 0;
        return -1;
    }
    SIZE Size;
    if (bCompRow && !IMEManager->IsNotComposing() && Line == CaretLine)
    {
        if (!IsWindowedIME(GConsole->GetIMEType()))
        {
            INT CompLen = Comp.Len();
            for (INT i = Start; i < CompLen; i++)
            {
                NCGDevice::GetTextExtent(&Size, *Comp.Mid(i, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                X += Size.cx;
                if (X > (INT)Width - 36)
                    return i;
            }
        }
        Start = CaretCol;
    }
    INT LineLen = Lines.GetLine(Line)->Text.Len();
    for (INT Col = Start; Col < LineLen + 1; Col++)
    {
        if (Line == CaretLine && !bCompRow && Col == CaretCol && !IMEManager->IsNotComposing() &&
            !IsWindowedIME(GConsole->GetIMEType()) && CaretCol != Start)
        {
            for (INT k = 0; k < Comp.Len(); k++)
            {
                NCGDevice::GetTextExtent(&Size, *Comp.Mid(k, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                X += Size.cx;
                if (X > (INT)Width - 36)
                {
                    bCompRow = 1;
                    return k;
                }
            }
        }
        if (Lines.GetLine(Line))
            NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                     L2FONT_Normal);
        else
            Size.cx = 0;
        X += Size.cx;
        if (X > (INT)Width - 36)
        {
            bCompRow = 0;
            return Col;
        }
    }
    if (Line + 1 == CaretLine && CaretCol == 0 && !IMEManager->IsNotComposing())
        bCompRow = 1;
    else
        bCompRow = 0;
    return 0;
}
NW_HOOK_METHOD(0x10027420, NCMultiEdit, GetRowEnd)

// 0x10027800 - line of a screen row (-1 past the text); unlike GetRowStart it does not count the position after
// a line's last character
INT NCMultiEdit::GetRowLine(INT Row)
{
    INT CurRow = 0;
    if (Row >= TotalRows || Lines.Num() <= 0)
        return -1;
    INT Line = 0;
    do
    {
        if (CurRow == Row)
            return Line;
        INT X = 0;
        INT LineLen = 0;
        if (Lines.GetLine(Line))
            LineLen = Lines.GetLine(Line)->Text.Len();
        for (INT Col = 0; Col < LineLen; Col++)
        {
            SIZE Size;
            if (Line == CaretLine && Col == CaretCol && !IMEManager->IsNotComposing() &&
                !IsWindowedIME(GConsole->GetIMEType()))
            {
                for (INT k = 0; k < Comp.Len(); k++)
                {
                    NCGDevice::GetTextExtent(&Size, *Comp.Mid(k, 1), 0, 0, 0, 0, 0, L2FONT_Normal);
                    X += Size.cx;
                    if (X > (INT)Width - 36)
                    {
                        X = 0;
                        if (++CurRow == Row)
                            return Line;
                        k--;
                    }
                }
            }
            if (Lines.GetLine(Line))
                NCGDevice::GetTextExtent(&Size, *Lines.GetLine(Line)->Text.Mid(Col, 1), 0, 0, 0, 0, 0,
                                         L2FONT_Normal);
            else
                Size.cx = 0;
            X += Size.cx;
            if (X > (INT)Width - 36)
            {
                X = 0;
                if (++CurRow == Row)
                    return Line;
                Col--;
            }
        }
        CurRow++;
        Line++;
    } while (Line < Lines.Num());
    return -1;
}
NW_HOOK_METHOD(0x10027800, NCMultiEdit, GetRowLine)

// 0x10027b10 (vslot 80)
INT NCMultiEdit::OnResult(WPARAM wParam, LPARAM lParam)
{
    bReading = 0;
    Reading.Empty();
    TCHAR* Str = IMEManager->GetResultStr();
    if (Str)
    {
        InsertString(Str, 1);
        return 1;
    }
    Comp.Empty();
    return 1;
}
NW_HOOK_METHOD(0x10027b10, NCMultiEdit, OnResult)

// 0x10027b60 (vslot 72) - the Chinese layouts show the composition in the IME's window, whose
// caret follows the length change
INT NCMultiEdit::OnComposition(WPARAM wParam, LPARAM lParam)
{
    bReading = 0;
    Reading.Empty();
    TCHAR* Str = IMEManager->GetCompStr();
    if (Str)
        InsertString(Str, 0);
    else
        Comp.Empty();
    if (GConsole->GetIMEType() == 10 || GConsole->GetIMEType() == 9)
    {
        NCCompositeBox* Box = ChineseCompositeBox(IMEManager);
        FString Old(ChineseLastComp(IMEManager));
        Box->SetText(*Comp);
        if (Old.Len() > Comp.Len())
            Box->CaretLeft();
        else if (Old.Len() < Comp.Len())
            Box->CaretRight();
        Box->SetText(*Comp);
    }
    return 1;
}
NW_HOOK_METHOD(0x10027b60, NCMultiEdit, OnComposition)

// 0x10027ca0 - the lines joined (their '\n' included) in a buffer the box keeps; NULL when
// empty
TCHAR* NCMultiEdit::GetText()
{
    if (TextBuf)
    {
        delete[] TextBuf;
        TextBuf = NULL;
    }
    UINT Total = Lines.TotalLength();
    if (!Total)
        return NULL;
    TextBuf = new TCHAR[Total + 1];
    INT N = Lines.Num();
    INT Pos = 0;
    for (INT i = 0; i < N; i++)
    {
        INT Length = 0;
        if (Lines.GetLine(i))
        {
            Length = Lines.GetLine(i)->Text.Len();
            appMemcpy(TextBuf + Pos, *Lines.GetLine(i)->Text, Length * sizeof(TCHAR));
        }
        Pos += Length;
    }
    TextBuf[Pos] = 0;
    return TextBuf;
}
NW_HOOK_METHOD(0x10027ca0, NCMultiEdit, GetText)

// 0x10028030 - a character at the caret; '\n' or '\r' ends the line there (and stays at its end), the rest of it
// going to a new line. C2: the width is measured on the formatted string (not on the code), CountRows counts the
// composition (bComp).
void NCMultiEdit::InsertChar(TCHAR Ch)
{
    FString Right;
    FString Left;
    TCHAR Buf[260];
    appSprintf(Buf, TEXT("%c"), Ch);
    FString Str(Buf);
    if (Ch == '\n')
    {
        Lines.Insert(CaretLine + 1);
        if (Lines.GetLine(CaretLine + 1) && Lines.GetLine(CaretLine))
        {
            Lines.GetLine(CaretLine + 1)->Text =
                Lines.GetLine(CaretLine)->Text.Right(Lines.GetLine(CaretLine)->Text.Len() - CaretCol);
            Lines.GetLine(CaretLine)->Text = Lines.GetLine(CaretLine)->Text.Left(CaretCol) + Str;
        }
        CaretLine++;
        NumLines = Lines.Num();
        CaretCol = 0;
        CaretX = 0;
        CaretRow++;
        TotalRows++;
    }
    else
    {
        INT OldRows = CountRows(CaretLine, 1);
        if (Lines.GetLine(CaretLine))
        {
            Left = Lines.GetLine(CaretLine)->Text.Left(CaretCol);
            Right = Lines.GetLine(CaretLine)->Text.Right(Lines.GetLine(CaretLine)->Text.Len() - CaretCol);
            Lines.GetLine(CaretLine)->Text = Left + Str + Right;
        }
        CaretCol++;
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *Str, 0, 0, 0, 0, 0, L2FONT_Normal);
        CaretX += Size.cx;
        if (CaretX > (INT)Width - 36)
        {
            CaretRow++;
            CaretX = Size.cx;
        }
        UpdateRowCount(CaretLine, OldRows, 0);
    }
    ScrollToCaret();
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x10028030, NCMultiEdit, InsertChar)

// 0x10028450 - the characters one by one, as typed
void NCMultiEdit::SetText(const TCHAR* Text)
{
    INT Length = wcslen(Text);
    for (INT i = 0; i < Length; i++)
        InsertChar(Text[i]);
    ScrollToCaret();
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x10028450, NCMultiEdit, SetText)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100284a0 (vslot 38)
INT NCMultiEdit::OnDestroy()
{
    CaretLine = 0;
    CaretCol = 0;
    Lines.Clear();
    if (TextBuf)
    {
        delete[] TextBuf;
        TextBuf = NULL;
    }
    if (CaretTimer)
    {
        KillTimer(CaretTimer);
        CaretTimer = 0;
    }
    if (UpButton)
    {
        RemoveChildWindow(UpButton->Entry);
        if (UpButton)
            delete UpButton;
        UpButton = NULL;
    }
    if (DownButton)
    {
        RemoveChildWindow(DownButton->Entry);
        if (DownButton)
            delete DownButton;
        DownButton = NULL;
    }
    if (ScrollBar)
    {
        RemoveChildWindow(ScrollBar->Entry);
        if (ScrollBar)
            delete ScrollBar;
        ScrollBar = NULL;
    }
    if (IMEManager)
    {
        RemoveChildWindow(IMEManager->Entry);
        if (IMEManager)
            delete IMEManager;
        IMEManager = NULL;
    }
    NControl::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100284a0, NCMultiEdit, OnDestroy)

// 0x10028620 - one empty line again
void NCMultiEdit::Clear()
{
    CaretLine = 0;
    CaretCol = 0;
    Lines.Clear();
    Lines.Insert(-1);
    NumLines = 1;
    TotalRows = 1;
    CaretLine = 0;
    CaretCol = 0;
    TopLine = 0;
    CaretX = 0;
    CaretRow = 0;
    bCompRow = 0;
    bReading = 0;
    if (TextBuf)
    {
        delete[] TextBuf;
        TextBuf = NULL;
    }
}
NW_HOOK_METHOD(0x10028620, NCMultiEdit, Clear)

// The rows shown, from TopLine: the text of each row, the composition drawn at the caret by the
// painter of the client language, and the caret (Japanese: drawn apart). The width of what is
// drawn is kept across rows (TextW) and used where the stock code uses it. C3: no background texture is
// drawn (C4 does not even load one) and the text is 0xFFDCDCDC.
static FORCEINLINE void PaintMultiEditText(NCMultiEdit* E, NCGDevice* Device)
{
    INT Y = E->LineGap;
    INT TextW = 0;
    FString Seg;
    INT PainterOut = 0;
    INT Start = E->GetRowStart(E->TopLine);
    INT Line = E->GetRowLine(E->TopLine);
    for (INT Row = E->TopLine; Row < E->TopLine + E->VisibleRows; Row++)
    {
        if (Row >= E->TotalRows)
            break;
        INT RowNum = Row - E->TopLine + 1;
        INT End;
        if (E->bCompRow)
        {
            // the row starts inside the composition
            End = E->GetRowEnd(Start, Line);
            if (E->bCompRow)
            {
                // ... and ends inside it
                Seg = E->Comp.Mid(Start, End - Start);
                if (Seg.Len())
                {
                    if (GLanguageType == 2)
                    {
                        Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0, 0,
                                         0, 0, 0, 0);
                        E->PaintCompositionJP(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                    }
                    else if (GLanguageType == 3)
                    {
                        Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0, 0,
                                         0, 0, 0, 0);
                        E->PaintCompositionTW(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                    }
                    else if (GLanguageType == 4)
                        E->PaintCompositionCN(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                    else if (GLanguageType == 0)
                        E->PaintCompositionKR(Seg, 0, Y, Device);
                    else
                        Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0, 0,
                                         0, 0, 0, 0);
                }
            }
            else
            {
                // ... and the line's text follows the rest of it
                Seg = E->Comp.Mid(Start, E->Comp.Len() - Start);
                TextW = 0;
                if (GLanguageType == 2)
                {
                    TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0,
                                             0, 0, 0, 0, 0);
                    E->PaintCompositionJP(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                }
                else if (GLanguageType == 3)
                {
                    TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0,
                                             0, 0, 0, 0, 0);
                    E->PaintCompositionTW(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                }
                else if (GLanguageType == 4)
                    E->PaintCompositionCN(Seg, Start, 0, Y, RowNum, Device, &PainterOut);
                else if (GLanguageType == 0)
                {
                    TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0,
                                             0, 0, 0, 0, 0);
                    E->PaintCompositionKR(Seg, 0, Y, Device);
                }
                else
                    TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0,
                                             0, 0, 0, 0, 0);
                if (E->Lines.GetLine(Line))
                {
                    if (End > 0)
                        Seg = E->Lines.GetLine(Line)->Text.Mid(E->CaretCol, End - E->CaretCol);
                    else
                        Seg = E->Lines.GetLine(Line)->Text.Mid(
                            E->CaretCol, E->Lines.GetLine(Line)->Text.Len() - E->CaretCol);
                    if (Seg.Len())
                        Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *Seg, 0, 0, 0,
                                         L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                }
            }
            if (Row == E->CaretRow && E->bCaretOn && GLanguageType == 2)
                Device->DrawText(E->CaretX + E->LineGap - 3, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0,
                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        }
        else if (Row == E->CaretRow)
        {
            // the caret's row: the text before the caret, the composition, the rest
            TextW = 0;
            if (E->CaretX && E->Lines.GetLine(Line))
            {
                Seg = E->Lines.GetLine(Line)->Text.Mid(Start, E->CaretCol - Start);
                TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0, 0,
                                         0, 0, 0, 0);
            }
            if (!E->IMEManager->IsNotComposing())
            {
                End = E->GetRowEnd(Start, Line);
                if (E->bCompRow)
                {
                    // the composition goes on in the next row
                    if (GLanguageType == 2)
                    {
                        Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0,
                                         0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionJP(E->Comp.Mid(0, End), 0, TextW, Y, RowNum, Device,
                                              &PainterOut);
                    }
                    else if (GLanguageType == 3)
                    {
                        Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0,
                                         0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionTW(E->Comp.Mid(0, End), 0, TextW, Y, RowNum, Device,
                                              &PainterOut);
                    }
                    else if (GLanguageType == 4)
                        E->PaintCompositionCN(E->Comp.Mid(0, End), 0, TextW, Y, RowNum, Device,
                                              &PainterOut);
                    else
                        Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0,
                                         0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                }
                else
                {
                    INT CompW = 0;
                    if (GLanguageType == 2)
                    {
                        CompW = Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp, 0, 0, 0,
                                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionJP(E->Comp, 0, TextW, Y, RowNum, Device, &PainterOut);
                    }
                    else if (GLanguageType == 3)
                    {
                        CompW = Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp, 0, 0, 0,
                                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionTW(E->Comp, 0, TextW, Y, RowNum, Device, &PainterOut);
                    }
                    else if (GLanguageType == 4)
                    {
                        if (GConsole->GetIMEType() == 8)
                            CompW = Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp, 0, 0,
                                                     0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionCN(E->Comp, 0, TextW, Y, RowNum, Device, &PainterOut);
                    }
                    else if (GLanguageType == 0)
                    {
                        CompW = Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp, 0, 0, 0,
                                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                        E->PaintCompositionKR(E->Comp, TextW, Y, Device);
                    }
                    else
                        CompW = Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp, 0, 0, 0,
                                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                    TextW += CompW;
                    if (E->Lines.GetLine(Line))
                    {
                        if (End > 0)
                            Seg = E->Lines.GetLine(Line)->Text.Mid(E->CaretCol, End - E->CaretCol);
                        else
                            Seg = E->Lines.GetLine(Line)->Text.Mid(
                                E->CaretCol, E->Lines.GetLine(Line)->Text.Len() - E->CaretCol);
                        if (Seg.Len())
                            Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *Seg, 0, 0, 0,
                                             L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                    }
                }
                if (E->IsFocused() && E->bCaretOn && GLanguageType == 2)
                    Device->DrawText(E->CaretX + E->LineGap, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            }
            else
            {
                // C2: the manager and its Chinese IME are looked at first
                if (GConsole->GetIMEType() == 10 || GConsole->GetIMEType() == 9 ||
                    GConsole->GetIMEType() == 11 || GConsole->GetIMEType() == 8 ||
                    GConsole->GetIMEType() == 12)
                {
                    if (E->IMEManager && E->IMEManager->ChineseIME)
                        ChineseCompositeBox(E->IMEManager)->HideWindow();
                }
                // the caret goes after the text drawn so far on this row (TextW), not at CaretX
                if (E->IsFocused() && E->bCaretOn)
                    Device->DrawText(E->LineGap + TextW - 3, Y, 0xFFDCDCDC, TEXT("|"), 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                End = E->GetRowEnd(Start, Line);
                if (E->Lines.GetLine(Line))
                {
                    if (Row + 1 < E->TotalRows && End)
                        Seg = E->Lines.GetLine(Line)->Text.Mid(E->CaretCol, End - E->CaretCol);
                    else
                        Seg = E->Lines.GetLine(Line)->Text.Mid(
                            E->CaretCol, E->Lines.GetLine(Line)->Text.Len() - E->CaretCol);
                    if (Seg.Len())
                        Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *Seg, 0, 0, 0,
                                         L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                }
            }
        }
        else if (Row + 1 < E->TotalRows)
        {
            End = E->GetRowEnd(Start, Line);
            if (E->bCompRow)
            {
                // the composition starts in this row, after the line's text
                if (E->Lines.GetLine(Line))
                {
                    if (E->CaretCol > 0)
                        Seg = E->Lines.GetLine(Line)->Text.Mid(Start, E->CaretCol - Start);
                    else
                        Seg = E->Lines.GetLine(Line)->Text.Mid(
                            Start, E->Lines.GetLine(Line)->Text.Len() - Start);
                    if (Seg.Len())
                        TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0,
                                                 L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                }
                if (GLanguageType == 2)
                {
                    Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                    E->PaintCompositionJP(E->Comp.Mid(0, End), 0, TextW, Y, RowNum, Device,
                                          &PainterOut);
                }
                else if (GLanguageType == 3)
                {
                    Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                    E->PaintCompositionTW(E->Comp.Mid(0, End), 0, TextW, Y, RowNum, Device,
                                          &PainterOut);
                }
                else
                    Device->DrawText(E->LineGap + TextW, Y, 0xFFDCDCDC, *E->Comp.Mid(0, End), 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            }
            else if (E->Lines.GetLine(Line))
            {
                if (End)
                    Seg = E->Lines.GetLine(Line)->Text.Mid(Start, End - Start);
                else
                    Seg = E->Lines.GetLine(Line)->Text.Mid(Start,
                                                           E->Lines.GetLine(Line)->Text.Len() - Start);
                if (Seg.Len())
                    TextW = Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal,
                                             0, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // the last row
            End = E->GetRowEnd(Start, Line);
            if (E->Lines.GetLine(Line))
            {
                Seg = E->Lines.GetLine(Line)->Text.Mid(Start, E->Lines.GetLine(Line)->Text.Len() - Start);
                if (Seg.Len())
                    Device->DrawText(E->LineGap, Y, 0xFFDCDCDC, *Seg, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                                     0, 0, 0);
            }
        }
        Y += E->CharH + E->LineGap;
        Start = End;
        if (End == 0 && (!E->bCompRow || E->CaretCol == 0))
            Line++;
    }
}

// 0x100286b0 (vslot 56)
INT NCMultiEdit::OnPaint(NCGDevice* Device)
{
    guard(NCMultiEdit::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    PaintMultiEditText(this, Device);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100286b0, NCMultiEdit, OnPaint)

// 0x10026c00 (name ours) - with the Thai keyboard (IME type 13) a Thai character is taken only where the Thai
// spelling allows it after the text of the caret's line up to the caret (see NCEditBox::IsThaiInputValid, which
// does the same on its Left piece); 0 for the rest and for every other keyboard or without a line
UBOOL NCMultiEdit::IsThaiInputValid(UINT Ch)
{
    TCHAR Prev = 0;
    FString Text;
    if (GConsole->GetIMEType() == 13 && Lines.GetLine(CaretLine))
    {
        if (Lines.GetLine(CaretLine)->Text.Len() < CaretCol)
            return 0;
        Text = Lines.GetLine(CaretLine)->Text.Left(CaretCol);
        switch (IMEManager->GetThaiCharType(Ch))
        {
        case 1:
        case 2:
        case 5:
        case 6:
        case 7:
            return 1;
        case 3:
            if (Text.Len() < 1)
                return 0;
            {
                TCHAR Last = (*Text.Right(1))[0];
                if (Text.Len() > 1)
                    Prev = (*Text.Right(2))[0];
                if (IMEManager->GetThaiCharType(Last) == 1 || IMEManager->GetThaiCharType(Last) == 2)
                    return 1;
                if (IMEManager->GetThaiCharType(Last) != 8 || Text.Len() < 2)
                    return 0;
                return IMEManager->GetThaiCharType(Prev) == 2;
            }
        case 4:
            if (Text.Len() < 1)
                return 0;
            Prev = (*Text.Right(1))[0];
            if (IMEManager->GetThaiCharType(Prev) == 1)
                return 1;
            return IMEManager->GetThaiCharType(Prev) == 2;
        case 8:
        case 9:
            if (Text.Len() < 1)
                return 0;
            Prev = (*Text.Right(1))[0];
            if (IMEManager->GetThaiCharType(Prev) == 1 || IMEManager->GetThaiCharType(Prev) == 2)
                return 1;
            return IMEManager->GetThaiCharType(Prev) == 4;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x10026c00, NCMultiEdit, IsThaiInputValid)

// 0x10029af0 (vslot 57) - printable codes, the upper characters with the IME type 9, and the Thai characters the
// spelling rules allow with the Thai keyboard (C2)
INT NCMultiEdit::OnChar(WPARAM wParam, LPARAM lParam)
{
    TCHAR Buf[20];
    if ((wParam >= 0x20 && wParam <= 0x7E) || (GConsole->GetIMEType() == 9 && wParam > 0x81) ||
        (GConsole->GetIMEType() == 13 && IsThaiInputValid(wParam)))
    {
        appSprintf(Buf, TEXT("%c"), wParam);  // not used
        InsertChar((TCHAR)wParam);
    }
    return 1;
}
NW_HOOK_METHOD(0x10029af0, NCMultiEdit, OnChar)

// 0x10029b70 (name ours) - the Down arrow: the caret goes to the row below, to the same cluster / character offset
// in it (or to its end when it is shorter). Thai: offsets count clusters, and the column is the sum of their
// characters. C2 simplified the C1 case analysis through LineLength.
void NCMultiEdit::MoveCaretDown()
{
    TArray<ThaiCharacter> Before;
    TArray<ThaiCharacter> Next;
    INT BeforeCount = 0;
    INT NextCount = 0;
    if (Lines.GetLine(CaretLine) && CaretRow < TotalRows - 1)
    {
        INT RowStart = GetRowStart(CaretRow);
        INT RowEnd = GetRowEnd(RowStart, CaretLine);
        INT NextEnd = GetRowEnd(RowEnd, CaretLine);
        if (GLanguageType == 5)
        {
            NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(RowStart, CaretCol - RowStart), Before, 0);
            BeforeCount = Before.Num();
        }
        else
            BeforeCount = CaretCol - RowStart;
        if (RowEnd == 0)
            CaretLine++;
        if (CaretRow > TotalRows - 3)
        {
            // the row below is the last one: it runs to the end of its line
            if (GLanguageType == 5)
            {
                NCGDevice::MakeThaiCharArray(
                    *Lines.GetLine(CaretLine)->Text.Mid(RowEnd, LineLength(Lines.GetLine(CaretLine)->Text) - RowEnd),
                    Next, 0);
                NextCount = Next.Num();
            }
            else
                NextCount = LineLength(Lines.GetLine(CaretLine)->Text) - RowEnd;
        }
        else if (GLanguageType == 5)
        {
            if (NextEnd != 0)
                NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(RowEnd, NextEnd - RowEnd), Next, 0);
            else
                NCGDevice::MakeThaiCharArray(
                    *Lines.GetLine(CaretLine)->Text.Mid(RowEnd, LineLength(Lines.GetLine(CaretLine)->Text) - RowEnd),
                    Next, 0);
            NextCount = Next.Num();
        }
        else if (NextEnd == 0)
            NextCount = LineLength(Lines.GetLine(CaretLine)->Text) - RowEnd;
        else
            NextCount = NextEnd - RowEnd;
        CaretRow++;
        if (BeforeCount > NextCount)
        {
            if (NextEnd != 0)
                CaretCol = NextEnd;
            else
                CaretCol = LineLength(Lines.GetLine(CaretLine)->Text);
        }
        else if (GLanguageType == 5)
        {
            CaretCol = RowEnd;
            for (INT i = 0; i < BeforeCount; i++)
                CaretCol += Next(i).CharCount();
        }
        else
            CaretCol = BeforeCount + RowEnd;
        if (CaretX > 0)
            SetCaretX(CaretLine, CaretCol);
    }
}
NW_HOOK_METHOD(0x10029b70, NCMultiEdit, MoveCaretDown)

// the keys that move the caret wait for the IME to finish composing (IME type 18 commits the composition first)
#define MULTI_MOVE_OK(IMEType) ((IMEType) == 18 || IMEManager->IsNotComposing() == 1 || (IMEType) == 9 || \
                                (IMEType) == 11 || (IMEType) == 10 || (IMEType) == 12)

// 0x1002a190 (vslot 59) - the caret keys; with the IME type 18 (and the Chinese ones for some keys) a pending
// composition is committed first. Delete is Right then Backspace. The Thai client moves by clusters (Left,
// Right, Up, Down, Delete), Up/Down go through GetRowStart / GetRowEnd, and the scroll bar is updated after the
// caret keys (slot 69 UpdateScrollBar). C3: Backspace joins a line with the previous one at its LineLength, and
// Delete always ends with the line count, ScrollToCaret and UpdateScrollBar
INT NCMultiEdit::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    INT IMEType = GConsole->GetIMEType();
    FString Ch;
    Ch.Empty();
    TArray<ThaiCharacter> ArrA;
    TArray<ThaiCharacter> ArrB;
    switch (wParam)
    {
    case VK_BACK:
        if (IMEType == 18 || IMEManager->IsNotComposing() == 1)
        {
            if (Comp.Len())
                Comp.Delete(0, Comp.Len());
            FString Left;
            FString Right;
            if (CaretCol < 1)
            {
                // joins the line to the previous one, whose '\n' goes
                if (CaretCol == 0 && CaretLine > 0 && Lines.GetLine(CaretLine) && Lines.GetLine(CaretLine - 1))
                {
                    CaretCol = LineLength(Lines.GetLine(CaretLine - 1)->Text);
                    Left = Lines.GetLine(CaretLine - 1)->Text.Left(Lines.GetLine(CaretLine - 1)->Text.Len() - 1);
                    Right = Lines.GetLine(CaretLine)->Text;
                    Lines.GetLine(CaretLine - 1)->Text = Left + Right;
                    Lines.RemoveAt(CaretLine);
                    CaretLine--;
                    NumLines--;
                }
            }
            else
            {
                if (Lines.GetLine(CaretLine))
                {
                    Left = Lines.GetLine(CaretLine)->Text.Left(CaretCol - 1);
                    Right = Lines.GetLine(CaretLine)->Text.Right(Lines.GetLine(CaretLine)->Text.Len() - CaretCol);
                    Lines.GetLine(CaretLine)->Text = Left + Right;
                }
                CaretCol--;
            }
            RecalcRows(0);
        }
        NumLines = Lines.Num();
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_RETURN:
        if (IMEType == 18)
        {
            if (Comp.Len())
                InsertString((TCHAR*)*Comp, 1);
            SetText(TEXT("\r\n"));       // sa-emu: the stock pushes 0x101A6288 = L"\r\n" (0D 00 0A 00) at 0x1002A248 / 0x1002A26D; this was L"\n" (the CR was lost)
        }
        else if (IMEManager->IsNotComposing() == 1)
            SetText(TEXT("\r\n"));
        break;

    case VK_END:
        if (MULTI_MOVE_OK(IMEType))
        {
            if (Comp.Len() && IMEType == 18)
                InsertString((TCHAR*)*Comp, 1);
            if (Lines.GetLine(CaretLine))
            {
                if (CaretRow + 1 < TotalRows)
                {
                    if (GetRowStart(CaretRow + 1) == 0)
                        CaretCol = Lines.GetLine(CaretLine)->Text.Len() - 1;
                    else
                        CaretCol = GetRowStart(CaretRow + 1);
                }
                else
                    CaretCol = Lines.GetLine(CaretLine)->Text.Len();
            }
        }
        SetCaretX(CaretLine, CaretCol);
        break;

    case VK_HOME:
        if (MULTI_MOVE_OK(IMEType))
        {
            if (Comp.Len() && IMEType == 18)
                InsertString((TCHAR*)*Comp, 1);
            CaretCol = GetRowStart(CaretRow);
        }
        CaretX = 0;
        break;

    case VK_LEFT:
        if (MULTI_MOVE_OK(IMEType))
        {
            INT Step;
            if (Comp.Len() && IMEType == 18)
            {
                Step = Comp.Len();
                InsertString((TCHAR*)*Comp, 1);
            }
            else if (GLanguageType == 5)
            {
                NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(0, CaretCol), ArrA, 0);
                if (ArrA.Num() > 0)
                    Step = ArrA(ArrA.Num() - 1).CharCount();
                else
                    Step = 0;
            }
            else
                Step = 1;
            if (CaretCol > 0)
            {
                Ch = Lines.GetLine(CaretLine)->Text.Mid(CaretCol - Step, Step);
                SIZE Size;
                NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                CaretCol -= Step;
                if (CaretX > 0)
                    CaretX -= Size.cx;
                else
                {
                    CaretRow--;
                    SetCaretX(CaretLine, CaretCol);
                }
            }
            else
            {
                if (CaretLine > 0)
                {
                    CaretLine--;
                    if (Lines.GetLine(CaretLine))
                        CaretCol = Lines.GetLine(CaretLine)->Text.Len() - 1;
                }
                if (CaretRow > 0)
                    CaretRow--;
                SetCaretX(CaretLine, CaretCol);
            }
        }
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_RIGHT:
        if (MULTI_MOVE_OK(IMEType))
        {
            if (Comp.Len() && IMEType == 18)
                InsertString((TCHAR*)*Comp, 1);
            if (Lines.GetLine(CaretLine))
            {
                INT Step;
                if (GLanguageType == 5)
                {
                    NCGDevice::MakeThaiCharArray(
                        *Lines.GetLine(CaretLine)->Text.Right(Lines.GetLine(CaretLine)->Text.Len() - CaretCol), ArrA, 0);
                    if (ArrA.Num() > 0)
                        Step = ArrA(0).CharCount();
                    else
                        Step = 0;
                }
                else
                    Step = 1;
                INT Col = CaretCol;
                UBOOL bAdvance = 0;
                if (CaretLine + 1 < NumLines)
                {
                    if (Col < Lines.GetLine(CaretLine)->Text.Len() - 1)
                    {
                        Ch = Lines.GetLine(CaretLine)->Text.Mid(Col, Step);
                        bAdvance = 1;
                    }
                    else
                    {
                        // past the line's '\n': the start of the next line
                        CaretLine++;
                        CaretRow++;
                        CaretX = 0;
                        CaretCol = 0;
                    }
                }
                else if (Col < Lines.GetLine(CaretLine)->Text.Len())
                {
                    Ch = Lines.GetLine(CaretLine)->Text.Mid(Col, Step);
                    bAdvance = 1;
                }
                if (bAdvance)
                {
                    SIZE Size;
                    NCGDevice::GetTextExtent(&Size, *Ch, 0, 0, 0, 0, 0, L2FONT_Normal);
                    CaretX += Size.cx;
                    if (CaretX > (INT)Width - 36)
                    {
                        CaretRow++;
                        CaretX = Size.cx;
                    }
                    CaretCol += Step;
                }
            }
        }
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_UP:
        if (IMEType == 18 || IMEManager->IsNotComposing() == 1)
        {
            if (Comp.Len())
                InsertString((TCHAR*)*Comp, 1);
            if (!Lines.GetLine(CaretLine))
                break;
            if (CaretRow > 0)
            {
                INT RowStart = GetRowStart(CaretRow);
                INT PrevStart = GetRowStart(CaretRow - 1);
                if (RowStart == 0)
                {
                    // the row above ends the previous line
                    if (GLanguageType == 5)
                    {
                        NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(RowStart, CaretCol), ArrA, 0);
                        NCGDevice::MakeThaiCharArray(
                            *Lines.GetLine(CaretLine - 1)->Text.Mid(
                                PrevStart, Lines.GetLine(CaretLine - 1)->Text.Len() - PrevStart - 1),
                            ArrB, 0);
                        if (ArrB.Num() < ArrA.Num())
                        {
                            INT PrevNum = Lines.GetLine(CaretLine - 1)->Text.Num();
                            if (PrevNum == 0 || PrevNum - 1 <= 0)
                            {
                                CaretCol = 0;
                                CaretLine--;
                            }
                            else
                            {
                                PrevNum = Lines.GetLine(CaretLine - 1)->Text.Num();
                                CaretCol = PrevNum == 0 ? -1 : PrevNum - 2;
                                CaretLine--;
                            }
                        }
                        else
                        {
                            CaretCol = PrevStart;
                            for (INT i = 0; i < ArrA.Num(); i++)
                                CaretCol += ArrB(i).CharCount();
                            CaretLine--;
                        }
                    }
                    else
                    {
                        INT PrevNum = Lines.GetLine(CaretLine - 1)->Text.Num();
                        if (PrevNum == 0 || PrevNum - 1 == 0)
                        {
                            CaretCol = 0;
                            CaretLine--;
                        }
                        else
                        {
                            INT PrevLen = Lines.GetLine(CaretLine - 1)->Text.Len();
                            if (PrevLen - PrevStart - 1 < CaretCol)
                            {
                                PrevNum = Lines.GetLine(CaretLine - 1)->Text.Num();
                                CaretCol = PrevNum == 0 ? -1 : PrevNum - 2;
                                CaretLine--;
                            }
                            else
                            {
                                CaretCol += PrevStart;
                                CaretLine--;
                            }
                        }
                    }
                }
                else if (GLanguageType == 5)
                {
                    NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(RowStart, CaretCol - RowStart), ArrA, 0);
                    NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Mid(PrevStart, RowStart - PrevStart),
                                                 ArrB, 0);
                    if (ArrA.Num() > ArrB.Num())
                        CaretCol = RowStart;
                    else
                    {
                        CaretCol = PrevStart;
                        for (INT i = 0; i < ArrA.Num(); i++)
                            CaretCol += ArrB(i).CharCount();
                    }
                }
                else if (CaretCol - RowStart < RowStart - PrevStart)
                    CaretCol = CaretCol - RowStart + PrevStart;
                else
                    CaretCol = RowStart;
                CaretRow--;
                if (CaretX > 0)
                    SetCaretX(CaretLine, CaretCol);
            }
        }
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_DOWN:
        if (IMEType == 18 || IMEManager->IsNotComposing() == 1)
        {
            if (Comp.Len())
                InsertString((TCHAR*)*Comp, 1);
            MoveCaretDown();
        }
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_DELETE:
        if ((IMEType == 18 || IMEManager->IsNotComposing() == 1) && Lines.GetLine(CaretLine))
        {
            INT Line = CaretLine;
            if (Line < Lines.Num() - 1 || Lines.GetLine(Line)->Text.Len() > CaretCol)
            {
                OnKeyDown(VK_RIGHT, 0);
                if (GLanguageType == 5)
                {
                    if (CaretCol == 0 && CaretLine > 0)
                        OnKeyDown(VK_BACK, 0);
                    else
                    {
                        NCGDevice::MakeThaiCharArray(*Lines.GetLine(CaretLine)->Text.Left(CaretCol), ArrA, 0);
                        INT Step;
                        if (ArrA.Num() != 0)
                            Step = ArrA(ArrA.Num() - 1).CharCount();
                        else
                            Step = 0;
                        Lines.GetLine(CaretLine)->Text.Delete(CaretCol - Step, Step);
                        CaretCol -= Step;
                        RecalcRows(0);
                        NumLines = Lines.Num();
                        ScrollToCaret();
                        UpdateScrollBar();
                    }
                }
                else
                    OnKeyDown(VK_BACK, 0);
            }
        }
        NumLines = Lines.Num();
        ScrollToCaret();
        UpdateScrollBar();
        break;

    case VK_PROCESSKEY:
        if (IMEType == 10)
        {
            if (lParam == 0x14B0001)
                ChineseCompositeBox(IMEManager)->CaretLeft();
            else if (lParam == 0x14D0001)
                ChineseCompositeBox(IMEManager)->CaretRight();
        }
        break;
    }
    return 1;
}
NW_HOOK_METHOD(0x1002a190, NCMultiEdit, OnKeyDown)

NW_CHECK_VTABLE(NCMultiEdit, 0x101a6138, 84)

/*-----------------------------------------------------------------------------
    NCHtmlMultiEdit.
-----------------------------------------------------------------------------*/

// 0x1004ae30 - NCMultiEdit's constructor, then the three scroll controls cleared
NCHtmlMultiEdit::NCHtmlMultiEdit()
{
    HtmlUpButton = NULL;
    HtmlDownButton = NULL;
    HtmlScrollBar = NULL;
}
NW_HOOK_CTOR(0x1004ae30, NCHtmlMultiEdit)

// 0x1004ae90 - the implicit destructor
NW_HOOK_DTOR(0x1004ae90, NCHtmlMultiEdit)


// 0x10047450 (vslot 25) - NCMultiEdit's (C2: no enabled test any more)
INT NCHtmlMultiEdit::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCMultiEdit::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10047450, NCHtmlMultiEdit, WindowProc)

// 0x10047470 (vslot 74)
const TCHAR* NCHtmlMultiEdit::GetValue()
{
    return GetText();
}
NW_HOOK_METHOD(0x10047470, NCHtmlMultiEdit, GetValue)

// 0x10047480 (vslot 73) - NCMultiEdit's, on the Html scroll bar
INT NCHtmlMultiEdit::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Delta = (SWORD)((SWORD)HIWORD(wParam) / 120);
    if (Delta > 0)
    {
        if (TopLine > Delta)
            TopLine -= Delta;
        else
            TopLine = 0;
    }
    else
    {
        INT D = Delta;
        if (TotalRows >= VisibleRows)
        {
            if (TotalRows + (D - TopLine) > VisibleRows)
                TopLine -= D;
            else
                TopLine = TotalRows - VisibleRows;
        }
        else if (TotalRows + (D - TopLine) > VisibleRows)
            TopLine -= D;
        else
            TopLine = 0;
    }
    HtmlScrollBar->SetValue(TopLine);
    return 1;
}
NW_HOOK_METHOD(0x10047480, NCHtmlMultiEdit, OnMouseWheel)

// 0x10047530 (vslot 76)
INT NCHtmlMultiEdit::OnScrollUp()
{
    if (TotalRows > VisibleRows)
    {
        if (--TopLine < 0)
            TopLine = 0;
        HtmlScrollBar->SetValue(TopLine);
    }
    return 1;
}
NW_HOOK_METHOD(0x10047530, NCHtmlMultiEdit, OnScrollUp)

// 0x10047570 (vslot 77)
INT NCHtmlMultiEdit::OnScrollDown()
{
    INT Visible = VisibleRows;
    if (TotalRows > Visible)
    {
        TopLine++;
        if (TopLine > TotalRows - Visible)
            TopLine = TotalRows - Visible;
        HtmlScrollBar->SetValue(TopLine);
    }
    return 1;
}
NW_HOOK_METHOD(0x10047570, NCHtmlMultiEdit, OnScrollDown)

// 0x100475c0 (vslot 75)
void NCHtmlMultiEdit::UpdateScrollBar()
{
    HtmlScrollBar->SetRange(TotalRows, VisibleRows);
    HtmlScrollBar->SetValue(TopLine);
}
NW_HOOK_METHOD(0x100475c0, NCHtmlMultiEdit, UpdateScrollBar)

// stock body shared with NCHtmlEdit's (0x10042a60, hooked there): the viewer scrolls the box into view
INT NCHtmlMultiEdit::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);
    NotifyParent(((DWORD)(_WORD)Local.Y << 16) | HTML_SCROLLTO);
    return 1;
}

// 0x1004b470 (vslot 37) - NCMultiEdit's, the IME manager made first. C3: the text height is measured on "A",
// the scroll controls are the L2UI_CH3 ones, 15 x 15 px inside the box's right edge
INT NCHtmlMultiEdit::OnCreate()
{
    TCHAR Name[128];
    TCHAR DisabledName[128];
    for (INT i = 1; i <= 9; i++)
    {
        appSprintf(Name, TEXT("l2ui_ch3.multiedit.M_inputbox0%d"), i);
        appSprintf(DisabledName, TEXT("l2ui_ch3.multiedit.M_inputbox0%d_disable"), i);
        BoxTex[i - 1] = FindTexture(Name);
        BoxDisabledTex[i - 1] = FindTexture(DisabledName);
    }
    IMEManager = new NCIMEManager(0);
    CreateChild(IMEManager, NWS_VISIBLE);
    WhiteTex = FindTexture(TEXT("NWindow.WhiteTexture"));
    CaretTimer = SetTimer(TIMER_MultiEditCaret, 500, 0);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, TEXT("A"), 0, 0, 0, 0, 0, L2FONT_Normal);
    CharH = Size.cy;
    LineGap = 2;
    CharW = Size.cx;
    CharH2 = Size.cy;
    NumLines = 1;
    VisibleRows = (INT)(Height - 2.0f) / (Size.cy + 2);
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);

    HtmlUpButton = new NCHtmlPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"));
    HtmlUpButton->ID = NID_MULTIEDIT_UP;
    HtmlUpButton->SetPosition((INT)(Local.W - 15.0f), 0, 15, 15);
    CreateChild(HtmlUpButton, NWS_VISIBLE);

    HtmlDownButton = new NCHtmlPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"), TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"));
    HtmlDownButton->ID = NID_MULTIEDIT_DOWN;
    HtmlDownButton->SetPosition((INT)(Local.W - 15.0f), (INT)(Local.H - 15.0f), 15, 15);
    CreateChild(HtmlDownButton, NWS_VISIBLE);

    HtmlScrollBar = new NCHtmlSliderBar(TEXT("L2UI_CH3.ScrollBar.SliderBarTop"), TEXT("L2UI_CH3.ScrollBar.SliderBarCenter"),
                                        TEXT("L2UI_CH3.ScrollBar.SliderBarBottom"));
    HtmlScrollBar->ID = NID_MULTIEDIT_BAR;
    HtmlScrollBar->SetPosition((INT)(Local.W - 15.0f), 15, 15, (INT)Local.H >= 30 ? (INT)(Local.H - 30.0f) : 0);
    CreateChild(HtmlScrollBar, NWS_VISIBLE);
    HtmlScrollBar->DoNothing(0);
    HtmlUpButton->RepeatDelay = 200;
    HtmlDownButton->RepeatDelay = 200;
    HtmlScrollBar->SetRange(TopLine, VisibleRows);
    HtmlScrollBar->SetValue(TopLine);
    return 1;
}
NW_HOOK_METHOD(0x1004b470, NCHtmlMultiEdit, OnCreate)

// 0x10047f50 (vslot 38)
INT NCHtmlMultiEdit::OnDestroy()
{
    Clear();
    if (CaretTimer)
    {
        KillTimer(CaretTimer);
        CaretTimer = 0;
    }
    if (HtmlUpButton)
    {
        RemoveChildWindow(HtmlUpButton->Entry);
        if (HtmlUpButton)
            delete HtmlUpButton;
        HtmlUpButton = NULL;
    }
    if (HtmlDownButton)
    {
        RemoveChildWindow(HtmlDownButton->Entry);
        if (HtmlDownButton)
            delete HtmlDownButton;
        HtmlDownButton = NULL;
    }
    if (HtmlScrollBar)
    {
        RemoveChildWindow(HtmlScrollBar->Entry);
        if (HtmlScrollBar)
            delete HtmlScrollBar;
        HtmlScrollBar = NULL;
    }
    if (IMEManager)
    {
        RemoveChildWindow(IMEManager->Entry);
        if (IMEManager)
            delete IMEManager;
        IMEManager = NULL;
    }
    NCMultiEdit::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10047f50, NCHtmlMultiEdit, OnDestroy)

// The first ancestor that is not a control (the stock walks the class chain inline against NControl's UClass, 0x102B8C88 in this
// client; NClasses.gen.h still has the C3 address)
static FORCEINLINE NCWnd* FirstNonControlParentC4(NCWnd* Wnd)
{
#if NW_HOOK_MODE
    UClass* ControlClass = (UClass*)::nw::Addr(0x102B8C88);
#else
    UClass* ControlClass = NClass::NControl();
#endif
    NWndEntry* E = Wnd->Entry;
    for (;;)
    {
        NCWnd* W = E->Parent->Wnd;
        if (!W)
            return W;
        UBOOL bControl = 0;
        for (UField* TempClass = reinterpret_cast<UField*>(W->GetClass()); TempClass; TempClass = TempClass->SuperField)
        {
            if (TempClass == reinterpret_cast<UField*>(ControlClass))
            {
                bControl = 1;
                break;
            }
        }
        if (!bControl)
            return W;
        E = W->Entry;
    }
}

// 0x100480a0 (vslot 62) - the box's frame (C4: nine pieces, 4 px corners and stretched edges and middle, the focused or the
// disabled set), clipped to its own width and the height of the first ancestor that is not a control; then the text as
// NCMultiEdit's
INT NCHtmlMultiEdit::OnPaint(NCGDevice* Device)
{
    guard(NCHtmlMultiEdit::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Owner = FirstNonControlParentC4(this);
    if (Owner)
        Device->SetClip(ScreenX, Owner->ScreenY, (INT)Width, (INT)Owner->Height);
    INT W = (INT)Width;
    INT H = (INT)Height;
    UTexture** Tex = IsFocused() ? BoxTex : BoxDisabledTex;
    if (Tex[0])
        Device->DrawTexture(0, 0, 4, 4, 0, 0, 4, 4, Tex[0]);
    if (Tex[1])
        Device->DrawStretchedTex(4, 0, W - 8, 4, Tex[1]);
    if (Tex[2])
        Device->DrawTexture(W - 4, 0, 4, 4, 0, 0, 4, 4, Tex[2]);
    if (Tex[3])
        Device->DrawStretchedTex(0, 4, 4, H - 8, Tex[3]);
    if (Tex[4])
        Device->DrawStretchedTex(4, 4, W - 8, H - 8, Tex[4]);
    if (Tex[5])
        Device->DrawStretchedTex(W - 4, 4, 4, H - 8, Tex[5]);
    if (Tex[6])
        Device->DrawTexture(0, H - 4, 4, 4, 0, 0, 4, 4, Tex[6]);
    if (Tex[7])
        Device->DrawStretchedTex(4, H - 4, W - 8, 4, Tex[7]);
    if (Tex[8])
        Device->DrawTexture(W - 4, H - 4, 4, 4, 0, 0, 4, 4, Tex[8]);
    PaintMultiEditText(this, Device);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100480a0, NCHtmlMultiEdit, OnPaint)

NW_CHECK_VTABLE(NCHtmlMultiEdit, 0x101aa248, 84)
