/*=============================================================================
    NHtmlLayout.cpp - the page tree of NCHtmlViewer: NHtmlBlock, NHtmlTable, NHtmlItem and
    the tag accessors. Stock: 0x10025d70 .. 0x1002b7e0, interleaved with the viewer. The
    parser is in NHtmlParser.cpp, the std::wstring members in NStl.cpp.
=============================================================================*/
#include "nwindow/NHtmlLayout.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/L2WordWrapperWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"

#include <new>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// "CENTER", "RIGHT", "LEFT", "TOP", "BOTTOM" (stock table, name ours)
#if NW_HOOK_MODE
#define GHtmlAlignNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10235328)))
#else
extern const TCHAR* const GHtmlAlignNames[];
#endif

// separator of NHtmlItem::Colors
enum { HTMLCOLOR_Sep = 0x1E };

// a colored stretch of text for NCGDevice's colored DrawText (12 bytes, name ours; the device
// takes them as the engine's FL2ColorFontInfo)
struct NHtmlColorRange
{
    INT Start;                 // first character
    INT End;                   // last character
    DWORD Color;
};

// top of an alignment stack, 0 when empty (inlined everywhere in the stock code)
static FORCEINLINE INT TopOf(TArray<INT>& Stack)
{
    return Stack.Num() > 0 ? Stack(Stack.Num() - 1) : 0;
}

// TArray<INT>::Pop, inline as in the stock build (with its check, UnTemplate.h line 398)
static FORCEINLINE INT PopAlign()
{
    return GHtmlAlignStack.Pop();
}

/*-----------------------------------------------------------------------------
    NHtmlTag.
-----------------------------------------------------------------------------*/

// 0x1003abb0 - the value of attribute Attr (the second argument is read but not used)
const TCHAR* NHtmlTag::GetAttr(INT Attr, UBOOL bUnused)
{
    _WORD* Id = AttrIds.First;
    NStlWString* Value = AttrValues.First;
    for (; Id != AttrIds.Last && Value != AttrValues.Last; Id++, Value++)
    {
        if (*Id == Attr)
            return Value->CStr();
    }
    return NULL;
}
NW_HOOK_METHOD(0x1003abb0, NHtmlTag, GetAttr)


// 0x1003ac10 - a bool: the stock callers test al (the rest of eax is what the loop left)
bool NHtmlTag::HasAttr(INT Attr)
{
    for (_WORD* Id = AttrIds.First; Id != AttrIds.Last; Id++)
    {
        if (*Id == Attr)
            return true;
    }
    return false;
}
NW_HOOK_METHOD(0x1003ac10, NHtmlTag, HasAttr)

// 0x1003b980 - "RRGGBB" read as 0xFFRRGGBB (a buffer from the global new)
DWORD HtmlParseColor(const TCHAR* Str)
{
    if (!Str)
        return 0;
    if (!wcscmp(Str, TEXT("LEVEL")))
        return 0xFFFFCC00;
    TCHAR* Buf = new TCHAR[wcslen(Str) + 5];
    wcscpy(Buf, TEXT("0xff"));
    wcscat(Buf, Str);
    Buf[wcslen(Str) + 4] = 0;
    DWORD Color = wcstoul(Buf, NULL, 16);
    delete[] Buf;
    return Color;
}
NW_HOOK_FUNC(0x1003b980, HtmlParseColor)

/*-----------------------------------------------------------------------------
    NHtmlObject.
-----------------------------------------------------------------------------*/

// 0x100326e0 - makes sure both alignment stacks have their bottom 0
NHtmlObject::NHtmlObject()
{
    Y = 0;
    X = 0;
    AlignedX = 0;
    Unknown14 = 0;
    MinWidth = 0;
    Height = 0;
    Width = 0;
    Type = 0;
    HAlign = 0;
    VAlign = 0;
    Parent = NULL;
    Viewer = NULL;
    OffsetY = 0;
    if (GHtmlAlignStack.Num() <= 0)
        GHtmlAlignStack.AddItem(0);
    if (GHtmlVAlignStack.Num() <= 0)
        GHtmlVAlignStack.AddItem(0);
}

// 0x1003dc20 - the abstract class' constructor body on a concrete stand-in (the stock constructors of the block, the
// table and the item call it, then store their own vtable): the vtable the stand-in stores is not the abstract one's
struct NHtmlObjectStandIn : public NHtmlObject
{
    virtual void Layout(INT MaxWidth, INT MaxHeight) {}
    virtual void Arrange(INT AreaWidth, INT AreaHeight) {}
    virtual void Draw(NCGDevice* Device) {}
    virtual UBOOL IsPercentWidth() { return 0; }
    virtual INT GetPercentWidth(INT Total) { return 0; }
    virtual void Release() {}
};
NW_HOOK_CTOR(0x1003dc20, NHtmlObjectStandIn)


// 0x1003ac50 - centered or right-aligned in an area AreaWidth wide (C2 also keeps the result
// in AlignedX)
void NHtmlObject::AlignX(INT AreaWidth)
{
    if (HAlign == HTMLALIGN_Center)
    {
        INT NewX = AreaWidth / 2 - Width / 2;
        AlignedX = NewX;
        X = NewX;
    }
    else if (HAlign == HTMLALIGN_Right)
    {
        INT NewX = AreaWidth - Width;
        AlignedX = NewX;
        X = NewX;
    }
}
NW_HOOK_METHOD(0x1003ac50, NHtmlObject, AlignX)

// 0x1003b930 - position in the page (the parents' positions added up)
POINT* NHtmlObject::GetAbsPos(POINT* Out)
{
    if (Parent)
    {
        POINT P;
        POINT* PP = Parent->GetAbsPos(&P);
        Out->x = X + PP->x;
        Out->y = Y + PP->y;
        return Out;
    }
    Out->x = X;
    Out->y = Y;
    return Out;
}
NW_HOOK_METHOD(0x1003b930, NHtmlObject, GetAbsPos)

/*-----------------------------------------------------------------------------
    NHtmlBlock.
-----------------------------------------------------------------------------*/

// 0x1003dca0 (the base of NHtmlTable)
NHtmlBlock::NHtmlBlock()
{
    WidthValue = 0;
    bPercent = 0;
    bBorder = 0;
    Mode = HTMLITEM_Text;
    Buffer[0] = 0;
    FixWidth = -1;
}
NW_HOOK_CTOR(0x1003dca0, NHtmlBlock)

// 0x1003dcf0
NHtmlBlock::NHtmlBlock(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InFont)
{
    WidthValue = 0;
    bPercent = 0;
    bBorder = 0;
    Mode = HTMLITEM_Text;
    Buffer[0] = 0;
    Parent = InParent;
    Viewer = InViewer;
    FixWidth = -1;
    Font = InFont;
}
NW_HOOK_CTOR_ARGS(0x1003dcf0, NHtmlBlock, (NHtmlObject* A, NCHtmlViewer* B, INT C), (A, B, C))

// 0x1003dd40 - the children go through NHtmlObject's destructor (Release has normally freed
// and removed them already)
NHtmlBlock::~NHtmlBlock()
{
    for (INT i = 0; i < Children.Num(); i++)
    {
        NHtmlObject* Child = Children(i);
        if (Child)
            delete Child;
    }
    Children.Empty();
}
NW_HOOK_DTOR(0x1003dd40, NHtmlBlock)

// 0x1003bab0 (vslot 0) - the children one under the other, at least MinWidth wide. Text
// items run on from where the previous one ended (FirstLineOffset) unless one of them is
// centered; with GL2ThaiHtmlMode the text of a run of text items goes to the word wrapper (the console's
// ThaiWordWrapperWnd, +0x3C1C: the stock reads this field, not the one at +0x3C2C).
void NHtmlBlock::Layout(INT MaxWidth, INT MaxHeight)
{
    INT N = Children.Num();
    INT ChildY = 0;
    if (MinWidth > MaxWidth)
        MaxWidth = MinWidth;
    SIZE Size;
    Width = MaxWidth;
    NCGDevice::GetCharSize(&Size, 'A', Font);
    for (INT i = 0; i < N; i++)
    {
        NHtmlObject* Child = Children(i);
        UBOOL bText = Child->Type == HTMLTAG_Text;
        UBOOL bNextText = 0;
        if (i < N - 1)
            bNextText = Children(i + 1)->Type == HTMLTAG_Text;
        if (GL2ThaiHtmlMode && bText)
        {
            if (!Buffer[0])
                appStrcpy(Buffer, ((NHtmlItem*)Child)->Text);
            if (bNextText)
                appStrcat(Buffer, ((NHtmlItem*)Children(i + 1))->Text);
            else
            {
                ((L2WordWrapperWnd*)GConsole->ThaiWordWrapperWnd)->WrapText(Buffer, MaxWidth);   // 0x10042df0
                Buffer[0] = 0;
            }
        }
        UBOOL bNextCenter = 0;
        if (i < N - 1)
            bNextCenter = Children(i + 1)->HAlign == HTMLALIGN_Center;
        UBOOL bCenter = Child->HAlign == HTMLALIGN_Center;
        UBOOL bPrevCenter = 0;
        if (i > 0 && bText)
        {
            NHtmlObject* Prev = Children(i - 1);
            bPrevCenter = Prev->HAlign == HTMLALIGN_Center;
            ((NHtmlItem*)Child)->FirstLineOffset = ((NHtmlItem*)Prev)->TailEnd;
            if (bCenter || bPrevCenter)
                ((NHtmlItem*)Child)->FirstLineOffset = 0;
        }
        Child->Layout(MaxWidth, MaxHeight);
        if (bText && !bPrevCenter)
        {
            Child->X = ((NHtmlItem*)Child)->FirstLineOffset;
            Child->Y = ChildY;
        }
        else
        {
            Child->X = 0;
            Child->Y = ChildY;
        }
        if (bText && bNextText && !bNextCenter && !bCenter)
            ChildY = ChildY + (Child->Height - Size.cy) - 6;
        else
            ChildY += Child->Height;
        if (Child->Width > Width)
            Width = Child->Width;
    }
    if (Height < ChildY)
        Height = ChildY;
}
NW_HOOK_METHOD(0x1003bab0, NHtmlBlock, Layout)

// 0x1003bcd0 (vslot 1) - fills the area; the children keep the places Layout gave them
void NHtmlBlock::Arrange(INT AreaWidth, INT AreaHeight)
{
    INT N = Children.Num();
    if (AreaHeight > Height)
        Height = AreaHeight;
    if (AreaWidth > Width)
        Width = AreaWidth;
    for (INT i = 0; i < N; i++)
        Children(i)->Arrange(Width, Height);
}
NW_HOOK_METHOD(0x1003bcd0, NHtmlBlock, Arrange)

// 0x1003bd20 (vslot 2) - a cell of a bordered table gets a white frame Border px thick,
// outside the cell
void NHtmlBlock::Draw(NCGDevice* Device)
{
    if (bBorder && Type == HTMLTAG_TD)
    {
        for (INT i = 0; i < Border; i++)
        {
            INT X1 = X - CellPadding - 1 + i;
            INT X2 = Width - i + CellPadding + X;
            INT Y1 = Y - CellPadding - 1 + i;
            INT Y2 = Height - i + Y + CellPadding;
            Device->DrawLine(X1, Y1, X2, Y1, 0xFFDCDCDC, 0);
            Device->DrawLine(X1, Y2, X2, Y2, 0xFFDCDCDC, 0);
            Device->DrawLine(X1, Y1, X1, Y2, 0xFFDCDCDC, 0);
            Device->DrawLine(X2, Y1, X2, Y2, 0xFFDCDCDC, 0);
        }
    }
    Device->PushOrigin(X, Y);
    INT N = Children.Num();
    for (INT i = 0; i < N; i++)
        Children(i)->Draw(Device);
    Device->PopOrigin();
}
NW_HOOK_METHOD(0x1003bd20, NHtmlBlock, Draw)

// 0x10002770 (vslot 3; the stock body `mov eax,[ecx+0x58]; ret` is shared with NCWnd::IsEnabled, which hooks it) -
// WIDTH was given as n%
UBOOL NHtmlBlock::IsPercentWidth()
{
    return bPercent;
}

// 0x1003ad20 (vslot 4)
INT NHtmlBlock::GetPercentWidth(INT Total)
{
    if (bPercent)
        return WidthValue * Total / 100;
    return 0;
}
NW_HOOK_METHOD(0x1003ad20, NHtmlBlock, GetPercentWidth)

// 0x1003cda0 (vslot 5)
void NHtmlBlock::Release()
{
    INT N = Children.Num();
    for (INT i = 0; i < N; i++)
    {
        Children(i)->Release();
        NHtmlObject* Child = Children(i);
        if (Child)
            delete Child;
    }
    Children.Empty();
}
NW_HOOK_METHOD(0x1003cda0, NHtmlBlock, Release)

// 0x1003dde0 - <td align valign width height fixwidth>: the alignment is pushed for the
// cell's contents (popped at </td>), a cell is vertically centered unless told otherwise
void NHtmlBlock::ParseCellAttrs(NHtmlTag* Tag)
{
    if (Tag->HasAttr(ATTR_TD_Align))
    {
        const TCHAR* Value = Tag->GetAttr(ATTR_TD_Align, 0);
        INT Align;
        if (!_wcsicmp(GHtmlAlignNames[0], Value))
            Align = HTMLALIGN_Center;
        else if (!_wcsicmp(GHtmlAlignNames[1], Value))
            Align = HTMLALIGN_Right;
        else
            Align = _wcsicmp(GHtmlAlignNames[2], Value) ? HTMLALIGN_None : HTMLALIGN_Left;
        GHtmlAlignStack.AddItem(Align);
    }
    else
        GHtmlAlignStack.AddItem(HTMLALIGN_None);

    if (Tag->HasAttr(ATTR_TD_VAlign))
    {
        const TCHAR* Value = Tag->GetAttr(ATTR_TD_VAlign, 0);
        if (!_wcsicmp(GHtmlAlignNames[3], Value))
            VAlign = HTMLVALIGN_Top;
        else if (!_wcsicmp(GHtmlAlignNames[4], Value))
            VAlign = HTMLVALIGN_Bottom;
        else
            VAlign = _wcsicmp(GHtmlAlignNames[0], Value) ? HTMLVALIGN_None : HTMLVALIGN_Center;
    }
    else
        VAlign = HTMLVALIGN_Center;

    if (Tag->HasAttr(ATTR_TD_Width))
    {
        const TCHAR* Value = Tag->GetAttr(ATTR_TD_Width, 0);
        WidthValue = NWtol(Value);
        if (Value[wcslen(Value) - 1] == '%')
            bPercent = 1;
    }
    if (Tag->HasAttr(ATTR_TD_Height))
        Height = NWtol(Tag->GetAttr(ATTR_TD_Height, 0));
    if (Tag->HasAttr(ATTR_TD_FixWidth))
        FixWidth = NWtol(Tag->GetAttr(ATTR_TD_FixWidth, 0));
    else
        FixWidth = -1;
    if (!bPercent && WidthValue > 0)
        Width = WidthValue;
}
NW_HOOK_METHOD(0x1003dde0, NHtmlBlock, ParseCellAttrs)

// 0x1003c450
INT NHtmlBlock::GetContentHeight()
{
    INT Total = 0;
    for (INT i = 0; i < Children.Num(); i++)
        Total += Children(i)->Height;
    return Total;
}
NW_HOOK_METHOD(0x1003c450, NHtmlBlock, GetContentHeight)

// 0x100413c0 - the item takes the pending mode (link, control), which then goes back to text
__declspec(noinline) NHtmlObject* NHtmlBlock::NewItem(NHtmlTag*** It, INT InFont)
{
    void* Mem = appMalloc(sizeof(NHtmlItem), TEXT("new"));
    NHtmlItem* Item = Mem ? ::new (Mem) NHtmlItem(this, Viewer, Mode, InFont) : NULL;
    Item->Parse(It);
    Mode = HTMLITEM_Text;
    return Item;
}
NW_HOOK_METHOD(0x100413c0, NHtmlBlock, NewItem)

// 0x1003ed70
__declspec(noinline) NHtmlObject* NHtmlBlock::NewTable(NHtmlTag*** It, INT InFont)
{
    void* Mem = appMalloc(sizeof(NHtmlTable), TEXT("new"));
    NHtmlTable* Table = Mem ? ::new (Mem) NHtmlTable(this, Viewer, InFont) : NULL;
    Table->Parse(It);
    return Table;
}
NW_HOOK_METHOD(0x1003ed70, NHtmlBlock, NewTable)

// 0x10041450 - the children up to the closing tag of the one *It points at. The loop tests
// the tag it handled last, so the closing tag is consumed before it stops. Only the first
// item after an <a> is a link. <br> and <br1> both make a line break item.
void NHtmlBlock::Parse(NHtmlTag*** It, UBOOL bInBorder)
{
    HAlign = TopOf(GHtmlAlignStack);
    VAlign = TopOf(GHtmlVAlignStack);
    bBorder = bInBorder;
    NHtmlTag* Tag = **It;
    Type = Tag->Tag;
    if (Tag->Tag == HTMLTAG_TD)
        ParseCellAttrs(Tag);

    while (Tag->Tag != Type || Tag->bOpen)
    {
        Tag = **It;
        NHtmlObject* Child;
        switch (Tag->Tag)
        {
        case HTMLTAG_Text:
            if (Tag->Text.Size == 0)
            {
                (*It)++;
                continue;
            }
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_Br:
        case HTMLTAG_Br1:
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_Img:
            Mode = HTMLITEM_Image;
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_Button:
            Mode = HTMLITEM_Button;
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_MultiEdit:
            Mode = HTMLITEM_MultiEdit;
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_Edit:
            Mode = HTMLITEM_Edit;
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_ComboBox:
            Mode = HTMLITEM_ComboBox;
            Child = NewItem(It, Font);
            break;
        case HTMLTAG_Table:
            Child = NewTable(It, Font);
            break;
        case HTMLTAG_Center:
            if (Tag->bOpen)
                GHtmlAlignStack.AddItem(HTMLALIGN_Center);
            else
                PopAlign();
            (*It)++;
            continue;
        case HTMLTAG_A:
            if (Tag->bOpen)
            {
                if (Tag->HasAttr(ATTR_A_Link))
                {
                    Mode = HTMLITEM_FileLink;
                    wcscpy(GHtmlLinkTarget, Tag->GetAttr(ATTR_A_Link, 0));
                    (*It)++;
                    continue;
                }
                if (Tag->HasAttr(ATTR_A_Action))
                {
                    Mode = HTMLITEM_ActionLink;
                    wcscpy(GHtmlLinkTarget, Tag->GetAttr(ATTR_A_Action, 0));
                    if (Tag->HasAttr(ATTR_A_Msg))
                        wcscpy(GHtmlLinkMsg, Tag->GetAttr(ATTR_A_Msg, 0));
                    else
                        GHtmlLinkMsg[0] = 0;
                    (*It)++;
                    continue;
                }
                // C3: href (the target stays empty, the value goes where an action link keeps its message)
                if (Tag->HasAttr(ATTR_A_Href))
                {
                    Mode = HTMLITEM_FileLink2;
                    wcscpy(GHtmlLinkMsg, Tag->GetAttr(ATTR_A_Href, 0));
                    (*It)++;
                    continue;
                }
            }
            Mode = HTMLITEM_Text;
            GHtmlLinkTarget[0] = 0;
            (*It)++;
            continue;
        default:
            (*It)++;
            continue;
        }
        Children.AddItem(Child);
        if (Child->Width > Width)
            Width = Child->Width;
        if (Child->MinWidth > MinWidth)
            MinWidth = Child->MinWidth;
    }

    if (Type == HTMLTAG_TD)
    {
        if (FixWidth > 0)
            Width = FixWidth;
        PopAlign();
    }
}
NW_HOOK_METHOD(0x10041450, NHtmlBlock, Parse)

/*-----------------------------------------------------------------------------
    NHtmlTable.
-----------------------------------------------------------------------------*/

// 0x1003dfb0
NHtmlTable::NHtmlTable(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InFont)
{
    NumRows = 0;
    CurCol = 0;
    Border = 0;
    bBgColor = 0;
    Font = InFont;
    Parent = InParent;
    Viewer = InViewer;
    BgColor = 0xFFFFFFFF;
    CellSpacing = 2;
    CellPadding = 1;
}
NW_HOOK_CTOR_ARGS(0x1003dfb0, NHtmlTable, (NHtmlObject* A, NCHtmlViewer* B, INT C), (A, B, C))

// 0x1003ce00 (vslot 0) - each column as wide as its widest cell (scaled to a set width), each
// row as high as its highest cell, cells aligned in their row; a table with a height stretches
// its rows to it
void NHtmlTable::Layout(INT MaxWidth, INT MaxHeight)
{
    RowHeights.Empty();
    INT N = Rows.Num();
    INT TableWidth = 0;
    INT RowHeight = 0;
    INT SumRowHeights = 0;
    INT CellY = CellPadding + Border * 2 + CellSpacing;
    if (bPercent)
        WidthValue = WidthValue * MaxWidth / 100;
    if (Width >= MaxWidth && WidthValue == 0)
        WidthValue = MaxWidth;
    for (INT Row = 0; Row < N; Row++)
    {
        TArray<NHtmlBlock*>* Cells = Rows(Row);
        INT NumCells = Cells->Num();
        INT CellX = CellPadding + Border * 2 + CellSpacing;
        for (INT Col = 0; Col < NumCells; Col++)
        {
            NHtmlBlock* Cell = (*Cells)(Col);
            if (MaxWidth > Width && WidthValue == 0)
                Cell->Layout(ColWidths(Col), CellY);
            else
            {
                INT W = ColWidths(Col) * WidthValue;
                if (Width > 0)
                    W = W / Width;
                Cell->Layout(W, CellY);
            }
            Cell->Y = CellY;
            Cell->X = CellX;
            Cell->Height -= 6;
            if (Cell->Height > RowHeight)
                RowHeight = Cell->Height;
            INT CellW = Cell->Width;
            CellX += CellW + (Border + CellPadding) * 2 + CellSpacing;
            if (Col < ColLayoutWidths.Num())
            {
                if (CellW > ColLayoutWidths(Col))
                    ColLayoutWidths(Col) = CellW;
            }
            else
                ColLayoutWidths.AddItem(CellW);
        }
        for (INT Col = 0; Col < NumCells; Col++)
        {
            NHtmlBlock* Cell = (*Cells)(Col);
            if (Cell->VAlign == HTMLVALIGN_Center)
                Cell->OffsetY = RowHeight / 2 - Cell->Height / 2;
            else if (Cell->VAlign == HTMLVALIGN_Bottom)
                Cell->OffsetY = RowHeight - Cell->Height;
        }
        RowHeights.AddItem(RowHeight);
        SumRowHeights += RowHeight;
        CellY += RowHeight + (CellPadding + Border) * 2 + CellSpacing;
        if (CellX - CellPadding > TableWidth)
            TableWidth = CellX - CellPadding;
        RowHeight = 0;
    }

    if (CellY - CellPadding >= Height)
    {
        Height = CellY - CellPadding;
        Width = TableWidth;
        return;
    }
    // taller than its rows need: the rows share the extra height
    INT NumRowHeights = RowHeights.Num();
    INT Avail = Height - ((CellPadding + Border) * NumRowHeights + Border) * 2 -
                (NumRowHeights + 1) * CellSpacing;
    CellY = CellPadding + Border * 2 + CellSpacing;
    for (INT Row = 0; Row < N; Row++)
    {
        TArray<NHtmlBlock*>* Cells = Rows(Row);
        INT NumCells = Cells->Num();
        for (INT Col = 0; Col < NumCells; Col++)
        {
            INT NewH = RowHeights(Row) * Avail / SumRowHeights;
            NHtmlBlock* Cell = (*Cells)(Col);
            if (Cell->VAlign == HTMLVALIGN_Center)
                Cell->OffsetY = NewH / 2 - Cell->Height / 2;
            else if (Cell->VAlign == HTMLVALIGN_Bottom)
                Cell->OffsetY = NewH - Cell->Height;
            Cell->Height = NewH;
            Cell->Y = CellY;
        }
        RowHeights(Row) = (*Cells)(0)->Height;
        CellY += RowHeights(Row) + (CellPadding + Border) * 2 + CellSpacing;
    }
    Width = TableWidth;
}
NW_HOOK_METHOD(0x1003ce00, NHtmlTable, Layout)

// 0x1003be40 (vslot 1) - every cell of a column gets the column's width, every cell of a row
// the row's height
void NHtmlTable::Arrange(INT AreaWidth, INT AreaHeight)
{
    AlignX(AreaWidth);
    INT N = Rows.Num();
    for (INT Row = 0; Row < N; Row++)
    {
        TArray<NHtmlBlock*>* Cells = Rows(Row);
        INT NumCells = Cells->Num();
        INT CellX = CellSpacing + Border * 2 + CellPadding;
        for (INT Col = 0; Col < NumCells; Col++)
        {
            NHtmlBlock* Cell = (*Cells)(Col);
            Cell->X = CellX;
            INT ColW = ColLayoutWidths(Col);
            CellX += ColW + (Border + CellPadding) * 2 + CellSpacing;
            Cell->Arrange(ColW, RowHeights(Row));
        }
    }
}
NW_HOOK_METHOD(0x1003be40, NHtmlTable, Arrange)

// 0x1003bf10 (vslot 2) - background, a white frame Border px thick, cells
void NHtmlTable::Draw(NCGDevice* Device)
{
    if (bBgColor)
        Device->DrawRect(X, Y, Width, Height, BgColor);
    if (Border > 0)
    {
        for (INT i = 0; i < Border; i++)
        {
            INT X2 = Width - i - 1 + X;
            INT X1 = X + i;
            INT Y1 = Y + i;
            INT Y2 = Height - i - 1 + Y;
            Device->DrawLine(X1, Y1, X2, Y1, 0xFFDCDCDC, 0);
            Device->DrawLine(X1, Y2, X2, Y2, 0xFFDCDCDC, 0);
            Device->DrawLine(X1, Y1, X1, Y2, 0xFFDCDCDC, 0);
            Device->DrawLine(X2, Y1, X2, Y2, 0xFFDCDCDC, 0);
        }
    }
    Device->PushOrigin(X, Y);
    INT N = Rows.Num();
    for (INT Row = 0; Row < N; Row++)
    {
        INT NumCells = Rows(Row)->Num();
        for (INT Col = 0; Col < NumCells; Col++)
            (*Rows(Row))(Col)->Draw(Device);
    }
    Device->PopOrigin();
}
NW_HOOK_METHOD(0x1003bf10, NHtmlTable, Draw)

// 0x1003e480 (vslot 5)
void NHtmlTable::Release()
{
    INT N = Rows.Num();
    for (INT Row = 0; Row < N; Row++)
    {
        INT NumCells = Rows(Row)->Num();
        for (INT Col = 0; Col < NumCells; Col++)
        {
            (*Rows(Row))(Col)->Release();
            NHtmlObject* Cell = (*Rows(Row))(Col);
            if (Cell)
                delete Cell;
        }
        Rows(Row)->Empty();
        TArray<NHtmlBlock*>* Cells = Rows(Row);
        if (Cells)
            delete Cells;
    }
    Rows.Empty();
    ColMinWidths.Empty();
    ColLayoutWidths.Empty();
    ColWidths.Empty();
    RowHeights.Empty();
}
NW_HOOK_METHOD(0x1003e480, NHtmlTable, Release)

// 0x1003e070 - a <td>: the cells draw their frame when the table has a border
__declspec(noinline) NHtmlBlock* NHtmlTable::NewCell(NHtmlTag*** It, INT InFont)
{
    void* Mem = appMalloc(sizeof(NHtmlBlock), TEXT("new"));
    NHtmlBlock* Cell = Mem ? ::new (Mem) NHtmlBlock(this, Viewer, InFont) : NULL;
    if (Border > 0)
        Cell->Parse(It, 1);
    else
        Cell->Parse(It, 0);
    return Cell;
}
NW_HOOK_METHOD(0x1003e070, NHtmlTable, NewCell)

// 0x1003e110 - the rows and cells up to </table>, which is consumed (the loop tests the tag it
// handled last). A <td> before the first <tr> is never consumed: the stock loop spins on it.
void NHtmlTable::Parse(NHtmlTag*** It)
{
    HAlign = TopOf(GHtmlAlignStack);
    VAlign = TopOf(GHtmlVAlignStack);
    TArray<NHtmlBlock*>* Row = NULL;
    NHtmlTag* Tag = **It;
    Type = Tag->Tag;
    ColWidths.Empty();
    ColLayoutWidths.Empty();
    ColMinWidths.Empty();
    if (Tag->HasAttr(ATTR_Table_Border))
        Border = NWtol(Tag->GetAttr(ATTR_Table_Border, 0));
    if (Tag->HasAttr(ATTR_Table_Width))
    {
        const TCHAR* Value = Tag->GetAttr(ATTR_Table_Width, 0);
        WidthValue = NWtol(Value);
        if (Value[wcslen(Value) - 1] == '%')
            bPercent = 1;
    }
    if (Tag->HasAttr(ATTR_Table_Height))
        Height = NWtol(Tag->GetAttr(ATTR_Table_Height, 0));
    if (Tag->HasAttr(ATTR_Table_BgColor))
    {
        BgColor = HtmlParseColor(Tag->GetAttr(ATTR_Table_BgColor, 0));
        bBgColor = 1;
    }
    else
        bBgColor = 0;
    if (Tag->HasAttr(ATTR_Table_CellPadding))
        CellPadding = NWtol(Tag->GetAttr(ATTR_Table_CellPadding, 0));
    if (Tag->HasAttr(ATTR_Table_CellSpacing))
        CellSpacing = NWtol(Tag->GetAttr(ATTR_Table_CellSpacing, 0));

    while (Tag->Tag != Type || Tag->bOpen)
    {
        Tag = **It;
        if (Tag->Tag == HTMLTAG_TR)
        {
            if (Tag->bOpen)
            {
                NumRows++;
                Row = (TArray<NHtmlBlock*>*)appMalloc(sizeof(TArray<NHtmlBlock*>), TEXT("new"));
                if (Row)
                    ::new (Row) TArray<NHtmlBlock*>;
                Rows.AddItem(Row);
                (*It)++;
            }
            else
            {
                CurCol = 0;
                (*It)++;
            }
        }
        else if (Tag->Tag == HTMLTAG_TD)
        {
            if (!Row)
                continue;
            NHtmlBlock* Cell = NewCell(It, Font);
            Cell->CellPadding = CellPadding;
            Cell->Border = Border;
            Row->AddItem(Cell);
            if (CurCol < ColWidths.Num())
            {
                if (Cell->Width > ColWidths(CurCol))
                    ColWidths(CurCol) = Cell->Width;
                if (Cell->MinWidth > ColMinWidths(CurCol))
                    ColMinWidths(CurCol) = Cell->MinWidth;
                CurCol++;
            }
            else
            {
                ColWidths.AddItem(Cell->Width - CellPadding * 2);
                ColMinWidths.AddItem(Cell->MinWidth - CellPadding * 2);
                CurCol++;
            }
        }
        else
            (*It)++;
    }

    Width = 0;
    for (INT i = 0; i < ColWidths.Num(); i++)
    {
        Width += ColWidths(i) + CellSpacing + (Border + CellPadding) * 2;
        MinWidth += ColMinWidths(i);
    }
    Width += CellSpacing + Border * 2;
    MinWidth += CellSpacing + Border * 2;
}
NW_HOOK_METHOD(0x1003e110, NHtmlTable, Parse)

/*-----------------------------------------------------------------------------
    NHtmlItem.
-----------------------------------------------------------------------------*/

// 0x10040840 - a link item takes the target and message of the <a> being read
NHtmlItem::NHtmlItem(NHtmlObject* InParent, NCHtmlViewer* InViewer, INT InMode, INT InFont)
{
    Parent = InParent;
    Colors = NULL;
    Tex = NULL;
    Viewer = InViewer;
    Text = NULL;
    Mode = InMode;
    if (Mode)
    {
        Action.Assign(GHtmlLinkTarget, wcslen(GHtmlLinkTarget));
        Var.Assign(GHtmlLinkMsg, wcslen(GHtmlLinkMsg));
    }
    FirstLineOffset = 0;
    TailEnd = 0;
    Font = InFont;
}
NW_HOOK_CTOR_ARGS(0x10040840, NHtmlItem, (NHtmlObject* A, NCHtmlViewer* B, INT C, INT D), (A, B, C, D))

// 0x1003ba10 (vslot 0) - text wraps at the width given (at least one character), its first
// line starting FirstLineOffset px in (none when centered); the end of its last line is kept
void NHtmlItem::Layout(INT MaxWidth, INT MaxHeight)
{
    SIZE CharSize;
    NCGDevice::GetCharSize(&CharSize, 'A', Font);
    if (Type == HTMLTAG_Text)
    {
        INT W = MaxWidth;
        if (MinWidth > W)
            W = MinWidth;
        if (HAlign == HTMLALIGN_Center)
            FirstLineOffset = 0;
        SIZE Size;
        DWORD End = NCGDevice::GetTextExtent(&Size, Text, W, 0, 1, 0, FirstLineOffset, (L2FontType)Font);
        Width = Size.cx;
        LayoutWidth = W;
        TailEnd = End;
        Height = Size.cy + 6;
        if (Mode == HTMLITEM_ActionLink || Mode == HTMLITEM_FileLink || Mode == HTMLITEM_FileLink2)
            TailWidth = End;
    }
}
NW_HOOK_METHOD(0x1003ba10, NHtmlItem, Layout)

// 0x10040260 (vslot 1) - aligned; a control item creates its control at its page position, an image finds its
// texture. The links also get where the item and its parent are, and (C4) the parent's vertical offset in
// Unknown14. The file link takes the message too (as the action link); mode 9 goes with the file links
void NHtmlItem::Arrange(INT AreaWidth, INT AreaHeight)
{
    AlignX(AreaWidth);
    POINT P;
    POINT PP;
    if (Mode == HTMLITEM_FileLink || Mode == HTMLITEM_FileLink2)
    {
        GetAbsPos(&P);
        PP.x = 0;
        PP.y = 0;
        if (Parent)
        {
            POINT Q;
            POINT* R = Parent->GetAbsPos(&Q);
            PP.x = R->x;
            PP.y = R->y;
            Unknown14 = Parent->OffsetY;
        }
        Viewer->AddFileLink(P.x, P.y, AlignedX, Unknown14, Width, Height, LayoutWidth, TailWidth,
                            PP.x, PP.y, Action.CStr(), Var.CStr());
    }
    else if (Mode == HTMLITEM_ActionLink)
    {
        GetAbsPos(&P);
        PP.x = 0;
        PP.y = 0;
        if (Parent)
        {
            POINT Q;
            POINT* R = Parent->GetAbsPos(&Q);
            PP.x = R->x;
            PP.y = R->y;
            Unknown14 = Parent->OffsetY;
        }
        Viewer->AddActionLink(P.x, P.y, AlignedX, Unknown14, Width, Height, LayoutWidth, TailWidth,
                              PP.x, PP.y, Action.CStr(), Var.CStr());
    }
    else if (Mode == HTMLITEM_Button)
    {
        GetAbsPos(&P);
        Viewer->AddButton(P.x, P.y, Width, Height, ForeTex.CStr(), BackTex.CStr(), Action.CStr(),
                          Var.CStr());
    }
    else if (Mode == HTMLITEM_MultiEdit)
    {
        GetAbsPos(&P);
        Viewer->AddMultiEdit(P.x, P.y, Width, Height, Var.CStr());
    }
    else if (Mode == HTMLITEM_Image)
        Tex = Viewer->FindPageTexture(Action.CStr());
    else if (Mode == HTMLITEM_Edit)
    {
        GetAbsPos(&P);
        Viewer->AddEdit(P.x, P.y, Width, Height, Var.CStr(), EditType, Action.CStr(), Length);
    }
    else if (Mode == HTMLITEM_ComboBox)
    {
        GetAbsPos(&P);
        Viewer->AddComboBox(P.x, P.y, Width, Height, Var.CStr(), Action.CStr(), Sel.CStr());
    }
}
NW_HOOK_METHOD(0x10040260, NHtmlItem, Arrange)

// 0x1003aef0 (anonymous in the db; NCHtmlViewer::FindPageTexture, whose stub in NCHtmlViewer.cpp enters it) - the page's
// texture of a name, the black texture when there is no name. Written here on the viewer as an NCObject
struct NHtmlViewerPageTexExt
{
    UTexture* Find(const TCHAR* Name);
};
UTexture* NHtmlViewerPageTexExt::Find(const TCHAR* Name)
{
    NCObject* Viewer = (NCObject*)this;
    if (Name)
        return Viewer->FindTexture(Name);
    return Viewer->FindTexture(TEXT("NWindow.BlackTexture"));
}
NW_HOOK_METHOD(0x1003aef0, NHtmlViewerPageTexExt, Find)

// 0x10041210 (vslot 2) - text white (links light blue and underlined, mode 6 red, <font color>
// runs in their colors), images stretched; a cell's vertical alignment moves it down
void NHtmlItem::Draw(NCGDevice* Device)
{
    INT ParentOffsetY = 0;
    if (Parent)
        ParentOffsetY = Parent->OffsetY;
    if (Type == HTMLTAG_Text)
    {
        if (Mode == HTMLITEM_Text)
        {
            if (wcscmp(Colors, TEXT("")))
            {
                DrawColoredText(Device, 0);
                return;
            }
            Device->DrawColoredLine(X, Y + ParentOffsetY, 0xFFDCDCDC, Text, LayoutWidth, 0, 1,
                                    (L2FontType)Font, 0, 0, 0, NULL, 0);
            return;
        }
        if (Mode != HTMLITEM_ActionLink && Mode != HTMLITEM_FileLink && Mode != HTMLITEM_FileLink2)
        {
            if (Mode == HTMLITEM_RedText)
                Device->DrawText(X, Y + ParentOffsetY, 0xFFFF0000, Text, LayoutWidth, 0, 1,
                                 (L2FontType)Font, 0, 0, 0, 0, 0, 1, 0);
            else
                Device->DrawText(X, Y + ParentOffsetY, 0xFFDCDCDC, Text, LayoutWidth, 0, 1,
                                 (L2FontType)Font, 0, 0, 0, 0, 0, 1, 0);
            return;
        }
        if (wcscmp(Colors, TEXT("")))
        {
            DrawColoredText(Device, 1);
            return;
        }
        Device->DrawColoredLine(X, Y + ParentOffsetY, 0xFF6699FF, Text, LayoutWidth, 0, 1,
                                (L2FontType)Font, 0, 1, 0xFF6699FF, NULL, 0);
    }
    else if (Type == HTMLTAG_Img && Tex)
        Device->DrawTexture(X, Y + ParentOffsetY, Width, Height, 0, 0, Width, Height, Tex);
}
NW_HOOK_METHOD(0x10041210, NHtmlItem, Draw)

// 0x10040670 (vslot 5) - a text item frees its copies; every item empties its strings
void NHtmlItem::Release()
{
    if (Type == HTMLTAG_Text)
    {
        if (Colors)
        {
            appFree(Colors);
            Colors = NULL;
        }
        if (Text)
        {
            appFree(Text);
            Text = NULL;
        }
    }
    TCHAR* It;
    Action.Erase(&It, Action.CStr(), Action.CStr() + Action.Size);
    ForeTex.Erase(&It, ForeTex.CStr(), ForeTex.CStr() + ForeTex.Size);
    BackTex.Erase(&It, BackTex.CStr(), BackTex.CStr() + BackTex.Size);
    Var.Erase(&It, Var.CStr(), Var.CStr() + Var.Size);
    Sel.Erase(&It, Sel.CStr(), Sel.CStr() + Sel.Size);
}
NW_HOOK_METHOD(0x10040670, NHtmlItem, Release)

// 0x1003ac90 - <edit type>: normal 0, password 2, number 3, date 4, time 5 (NCEditBox::Type)
INT NHtmlItem::ParseEditType(const TCHAR* Str)
{
    if (!wcscmp(Str, TEXT("normal")))
        return 0;
    if (!wcscmp(Str, TEXT("password")))
        return 2;
    if (!wcscmp(Str, TEXT("date")))
        return 4;
    if (!wcscmp(Str, TEXT("time")))
        return 5;
    return wcscmp(Str, TEXT("number")) ? 0 : 3;
}
NW_HOOK_METHOD(0x1003ac90, NHtmlItem, ParseEditType)

// 0x1003cc90 - a text run: its own copies of the text and of the color runs, measured on one
// line; the width of an "A" is the narrowest it gets
void NHtmlItem::SetText(NHtmlTag* Tag)
{
    Type = Tag->Tag;
    Text = (TCHAR*)appMalloc((wcslen(Tag->Text.CStr()) + 1) * sizeof(TCHAR), TEXT("new"));
    wcscpy(Text, Tag->Text.CStr());
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, (L2FontType)Font);
    Width = Size.cx;
    Height = Size.cy + 6;
    NCGDevice::GetCharSize(&Size, 'A', Font);
    MinWidth = Size.cx;
    Colors = (TCHAR*)appMalloc((wcslen(Tag->Colors.CStr()) + 1) * sizeof(TCHAR), TEXT("new"));
    wcscpy(Colors, Tag->Colors.CStr());
}
NW_HOOK_METHOD(0x1003cc90, NHtmlItem, SetText)

// 0x10040960 - the size and strings of the item from its tag, which is consumed. Only images
// and text record their tag in Type. Controls get a default size; edits and multi-line
// edits are 3 px taller than asked; <br1> is a line break with no height.
void NHtmlItem::Parse(NHtmlTag*** It)
{
    HAlign = TopOf(GHtmlAlignStack);
    VAlign = TopOf(GHtmlVAlignStack);
    NHtmlTag* Tag = **It;
    INT W, H;
    switch (Tag->Tag)
    {
    case HTMLTAG_Br1:
        Width = 1;
        MinWidth = 1;
        Height = 0;
        (*It)++;
        return;
    case HTMLTAG_Br:
        Width = 1;
        MinWidth = 1;
        Height = 9;
        (*It)++;
        return;
    case HTMLTAG_Text:
        SetText(Tag);
        (*It)++;
        return;
    case HTMLTAG_Button:
        W = 10;
        H = 10;
        if (Tag->HasAttr(ATTR_Button_Action))
            Action.Assign(Tag->GetAttr(ATTR_Button_Action, 0));
        if (Tag->HasAttr(ATTR_Button_Width))
            W = NWtol(Tag->GetAttr(ATTR_Button_Width, 0));
        if (Tag->HasAttr(ATTR_Button_Height))
            H = NWtol(Tag->GetAttr(ATTR_Button_Height, 0));
        ForeTex.Assign(Tag->HasAttr(ATTR_Button_Fore) ? Tag->GetAttr(ATTR_Button_Fore, 0)
                                                      : TEXT("NWindow.BlackTexture"));
        BackTex.Assign(Tag->HasAttr(ATTR_Button_Back) ? Tag->GetAttr(ATTR_Button_Back, 0)
                                                      : TEXT("NWindow.WhiteTexture"));
        if (Tag->HasAttr(ATTR_Button_Value))
            Var.Assign(Tag->GetAttr(ATTR_Button_Value, 0));
        Height = H;
        break;
    case HTMLTAG_MultiEdit:
        W = 50;
        H = 40;
        if (Tag->HasAttr(ATTR_MultiEdit_Width))
            W = NWtol(Tag->GetAttr(ATTR_MultiEdit_Width, 0));
        if (Tag->HasAttr(ATTR_MultiEdit_Height))
            H = NWtol(Tag->GetAttr(ATTR_MultiEdit_Height, 0));
        if (Tag->HasAttr(ATTR_MultiEdit_Var))
            Var.Assign(Tag->GetAttr(ATTR_MultiEdit_Var, 0));
        Height = H + 6;
        break;
    case HTMLTAG_Edit:
        W = 50;
        H = 15;
        if (Tag->HasAttr(ATTR_Edit_Width))
            W = NWtol(Tag->GetAttr(ATTR_Edit_Width, 0));
        if (Tag->HasAttr(ATTR_Edit_Height))
            H = NWtol(Tag->GetAttr(ATTR_Edit_Height, 0));
        if (Tag->HasAttr(ATTR_Edit_Var))
            Var.Assign(Tag->GetAttr(ATTR_Edit_Var, 0));
        if (Tag->HasAttr(ATTR_Edit_Action))
            Action.Assign(Tag->GetAttr(ATTR_Edit_Action, 0));
        EditType = 0;
        if (Tag->HasAttr(ATTR_Edit_Type))
            EditType = ParseEditType(Tag->GetAttr(ATTR_Edit_Type, 0));
        Length = 0;
        if (Tag->HasAttr(ATTR_Edit_Length))
            Length = appAtoi(Tag->GetAttr(ATTR_Edit_Length, 0));
        Height = H + 6;
        break;
    case HTMLTAG_ComboBox:
        W = 30;
        if (Tag->HasAttr(ATTR_Combo_Width))
            W = NWtol(Tag->GetAttr(ATTR_Combo_Width, 0));
        if (Tag->HasAttr(ATTR_Combo_Var))
            Var.Assign(Tag->GetAttr(ATTR_Combo_Var, 0));
        if (Tag->HasAttr(ATTR_Combo_List))
            Action.Assign(Tag->GetAttr(ATTR_Combo_List, 0));
        if (Tag->HasAttr(ATTR_Combo_Sel))
            Sel.Assign(Tag->GetAttr(ATTR_Combo_Sel, 0));
        Height = 21;
        break;
    case HTMLTAG_Img:
        Type = Tag->Tag;
        W = 0;
        H = 0;
        if (Tag->HasAttr(ATTR_Img_Src))
            Action.Assign(Tag->GetAttr(ATTR_Img_Src, 0));
        if (Tag->HasAttr(ATTR_Img_Width))
            W = NWtol(Tag->GetAttr(ATTR_Img_Width, 0));
        if (Tag->HasAttr(ATTR_Img_Height))
            H = NWtol(Tag->GetAttr(ATTR_Img_Height, 0));
        Height = H;
        break;
    default:
        (*It)++;
        return;
    }
    Width = W;
    MinWidth = W;
    (*It)++;
}
NW_HOOK_METHOD(0x10040960, NHtmlItem, Parse)

// 0x10040e10 - text with <font color> runs. Colors reads "\x1Ecolor\x1Eword\x1Ecolor\x1Eword" from
// its second character on; each word is looked for in a fresh copy of the whole text (C2: not
// after the previous one) and gets the color before it, the rest of the text stays white.
// bLink is not used (links lose their color). The ranges (the engine's TArray of pointers) are
// not freed, as in the stock code.
void NHtmlItem::DrawColoredText(NCGDevice* Device, UBOOL bLink)
{
    TCHAR* Spec = (TCHAR*)appMalloc((wcslen(Colors) + 2) * sizeof(TCHAR), TEXT("new"));
    wcscpy(Spec, Colors);
    Spec[wcslen(Colors)] = HTMLCOLOR_Sep;
    Spec[wcslen(Colors) + 1] = 0;
    INT SpecLen = wcslen(Spec);
    TCHAR* ColorBuf = (TCHAR*)appMalloc(0x2000, TEXT("new"));
    TCHAR* WordBuf = (TCHAR*)appMalloc(0x2000, TEXT("new"));
    INT ColorLen = 0;
    INT WordLen = 0;
    UBOOL bReadColor = 1;
    NStlWString Rest;
    Rest.Assign(Text, wcslen(Text));
    INT BaseY = 0;
    if (Parent)
        BaseY = Parent->OffsetY;
    POINT Pos;
    Pos.x = X;
    Pos.y = Y + BaseY;
    TArray<NHtmlColorRange*> Ranges;
    DWORD Color = 0;
    for (INT i = 1; i < SpecLen; i++)
    {
        TCHAR C = Spec[i];
        if (bReadColor)
        {
            if (C != HTMLCOLOR_Sep)
                ColorBuf[ColorLen++] = C;
            else
            {
                ColorBuf[ColorLen] = 0;
                Color = HtmlParseColor(ColorBuf);
                bReadColor = 0;
                ColorLen = 0;
            }
        }
        else
        {
            if (C != HTMLCOLOR_Sep)
                WordBuf[WordLen++] = C;
            else
            {
                WordBuf[WordLen] = 0;
                bReadColor = 1;
                WordLen = 0;
                NStlWString Copy;
                Copy.Assign(Text, wcslen(Text));
                INT Found = Copy.Find(WordBuf, 0, wcslen(WordBuf));
                if (Color)
                {
                    INT End = appStrlen(WordBuf) + Found - 1;
                    NHtmlColorRange* Range = (NHtmlColorRange*)appMalloc(sizeof(NHtmlColorRange), TEXT("new"));
                    Range->Start = Found;
                    Range->End = End;
                    Range->Color = Color;
                    Ranges.AddItem(Range);
                }
                Copy.Erase(0, wcslen(WordBuf) + Found);
            }
        }
    }
    Device->DrawColoredLine(Pos.x, Pos.y, 0xFFDCDCDC, Text, LayoutWidth, 0, 1, (L2FontType)Font, 0, 0,
                            0, (TArray<FL2ColorFontInfo*>*)&Ranges, 0);
    Ranges.Empty();
    Rest.Erase(0, Rest.Size);
    appFree(Spec);
    appFree(ColorBuf);
    appFree(WordBuf);
}
NW_HOOK_METHOD(0x10040e10, NHtmlItem, DrawColoredText)

// C2: no vtable for NW_CHECK_VTABLE(NHtmlBlock, 0x1011026c, 6)
// C2: no vtable for NW_CHECK_VTABLE(NHtmlTable, 0x10110284, 6)
// C2: no vtable for NW_CHECK_VTABLE(NHtmlItem, 0x10110254, 6)
