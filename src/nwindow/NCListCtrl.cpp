/*=============================================================================
    NCListCtrl.cpp - list with columns: cells, rows, column headers (NCHeaderCtrl) and the
    list itself. Stock (C3): 0x1001f3c0 .. 0x1001f6d0 (cells, column info, rows), 0x1001f6e0 ..
    0x1001f98d (the headers, the list's small members from 0x1001f990), 0x1001fb00 ..
    0x10021540 (the list).
=============================================================================*/
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"

#include <wchar.h>
#include <new>

// interface sounds (FL2NetNotify::OnInterfacePlaySound)
enum { SOUND_Click = 0, SOUND_ClickDisabled = 2 };

// the rows are 17 pixels high (C3; C2: 12 + LineGap)
enum { ROW_Height = 17 };

/*-----------------------------------------------------------------------------
    NListCell.
-----------------------------------------------------------------------------*/

// 0x10021b10
void NListCell::SetText(const TCHAR* InText)
{
    if (InText && *InText)
        appStrcpy(Text, InText);
    else
        Text[0] = 0;
}
NW_HOOK_METHOD(0x10021b10, NListCell, SetText)

// 0x10021b40
void NListCell::SetNumber(INT Value)
{
    appSprintf(Text, TEXT("%d"), Value);
}
NW_HOOK_METHOD(0x10021b40, NListCell, SetNumber)

// 0x10021b60 - NULL keeps the old picture
void NListCell::SetIcon(UTexture* InIcon, INT W, INT H)
{
    if (InIcon)
    {
        Icon = InIcon;
        IconW = W;
        IconH = H;
    }
}
NW_HOOK_METHOD(0x10021b60, NListCell, SetIcon)

// 0x10021c40
void NListCell::SetText2(const TCHAR* InText)
{
    if (InText && *InText)
        appStrcpy(Text2, InText);
    else
        Text2[0] = 0;
}
NW_HOOK_METHOD(0x10021c40, NListCell, SetText2)

// 0x10021c70
void NListCell::SetParams(INT InParam1, INT InParam2, INT InParam3)
{
    Param1 = InParam1;
    Param2 = InParam2;
    Param3 = InParam3;
}
NW_HOOK_METHOD(0x10021c70, NListCell, SetParams)

// 0x10021ca0
void NListCell::SetColor(DWORD InColor)
{
    Color = InColor;
}
NW_HOOK_METHOD(0x10021ca0, NListCell, SetColor)

// 0x100227a0 (anonymous in the db) - the cell as NListRow's constructor makes each one: no text, no picture (the sixteen
// extra pictures cleared), the grey color, -1 for the owner's two numbers, the text visible
NListCell::NListCell()
{
    Param1 = -1;
    Param2 = -1;
    Text[0] = 0;
    Icon = NULL;
    Color = 0xFFDCDCDC;
    Text2[0] = 0;
    UnknownA10 = 0;
    IconW = 0;
    IconH = 0;
    bTextVisible = 1;
    for (INT i = 0; i < 16; i++)
    {
        Images[i].Tex = NULL;
        Images[i].X = 0;
        Images[i].Y = 0;
        Images[i].W = 0;
        Images[i].H = 0;
        Images[i].U = 0;
        Images[i].V = 0;
        Images[i].UL = 0;
        Images[i].VL = 0;
    }
}
NW_HOOK_CTOR(0x100227a0, NListCell)

/*-----------------------------------------------------------------------------
    NListColumnInfo.
-----------------------------------------------------------------------------*/

// 0x10005ba0 (anonymous in the db; the linker folded it with every other setter of a dword at +8) - the title is a
// SysString index now
void NListColumnInfo::SetTitleID(INT InTitleID)
{
    TitleID = InTitleID;
}
NW_HOOK_METHOD(0x10005ba0, NListColumnInfo, SetTitleID)

// 0x10021cb0 (ret 8) - the normal state has three textures (the caps and the middle), picked by Index (0, 1, 2; another
// Index changes nothing)
void NListColumnInfo::SetNormalTex(const TCHAR* Name, INT Index)
{
    if (Index == 0)
    {
        if (Name && *Name)
            appStrcpy(NormalTex[0], Name);
        else
            NormalTex[0][0] = 0;
    }
    else if (Index == 1)
    {
        if (Name && *Name)
            appStrcpy(NormalTex[1], Name);
        else
            NormalTex[1][0] = 0;
    }
    else if (Index == 2)
    {
        if (Name && *Name)
            appStrcpy(NormalTex[2], Name);
        else
            NormalTex[2][0] = 0;
    }
}
NW_HOOK_METHOD(0x10021cb0, NListColumnInfo, SetNormalTex)

// 0x10021d40 (ret 8, guard: NCLVColumn::SetTabDownTex) - the pushed state: Index 0 .. 2 only (a bad Index is an error
// of the guard), the names 0x800 bytes apart
void NListColumnInfo::SetPushedTex(const TCHAR* Name, INT Index)
{
    guard(NCLVColumn::SetTabDownTex);
    if (Index >= 0 && Index <= 2)
    {
        if (Name && *Name)
            appStrcpy(PushedTex[Index], Name);
        else
            PushedTex[Index][0] = 0;
    }
    unguard;
}
NW_HOOK_METHOD(0x10021d40, NListColumnInfo, SetPushedTex)

/*-----------------------------------------------------------------------------
    NListRow.
-----------------------------------------------------------------------------*/

// 0x10023610 (ret 4) - the tool tip info is constructed first; the cells come from GMalloc in one block (no array header:
// they have no destructor), each made by NListCell's constructor (0x100227a0)
NListRow::NListRow(INT NumCells)
{
    NListCell* Block = (NListCell*)GMalloc->Malloc(NumCells * sizeof(NListCell), TEXT("new"));
    if (Block)
    {
        for (INT i = 0; i < NumCells; i++)
            new (&Block[i]) NListCell();
    }
    Param1 = -1;
    Param2 = -1;
    Cells = Block;
    Text[0] = 0;
}
NW_HOOK_CTOR_ARGS(0x10023610, NListRow, (INT A), (A))

// 0x100236c0 - the cells are freed, then the tool tip goes
NListRow::~NListRow()
{
    if (Cells)
    {
        GMalloc->Free(Cells);
        Cells = NULL;
    }
}
NW_HOOK_DTOR(0x100236c0, NListRow)

// 0x10021e00
void NListRow::SetText(const TCHAR* InText)
{
    if (InText && *InText)
        appStrcpy(Text, InText);
    else
        Text[0] = 0;
}
NW_HOOK_METHOD(0x10021e00, NListRow, SetText)

// 0x10021e30
void NListRow::SetParams(INT InParam1, INT InParam2, INT InParam3)
{
    Param1 = InParam1;
    Param2 = InParam2;
    Param3 = InParam3;
}
NW_HOOK_METHOD(0x10021e30, NListRow, SetParams)

// C1 0x10018910: no stock function in C2 (the tool tip is an NToolTipInfo the skin draws): the first line of
// text, NULL when there is none
const TCHAR* NListRow::GetToolTip()
{
    const TCHAR* Tip = *ToolTip.Name;
    if (!Tip || !*Tip)
        return NULL;
    return Tip;
}

/*-----------------------------------------------------------------------------
    NCHeaderCtrl.
-----------------------------------------------------------------------------*/

// 0x10023860 - NControl's constructor is inlined; the textures set by SetColumnInfo and TitleID are left as allocated
NCHeaderCtrl::NCHeaderCtrl()
{
    bClickable = 1;
    bAscending = 1;
    WholeNormalTex = NULL;
    WholePushedTex = NULL;
    WholeOverTex = NULL;
    State = 0;
    bNumeric = 0;
    ColumnWidth = 0;
    ColumnHeight = 0;
}
NW_HOOK_CTOR(0x10023860, NCHeaderCtrl)

// 0x10023920
NW_HOOK_DTOR(0x10023920, NCHeaderCtrl)

// stock body shared with NCButton's (0x100127a0)
INT NCHeaderCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10022020 (vslot 38) - guarded
INT NCHeaderCtrl::OnDestroy()
{
    guard(NCHeaderCtrl::OnDestroy);
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10022020, NCHeaderCtrl, OnDestroy)

// 0x10022820 (vslot 62) - the header's own texture stretched over it (67 x 15 texels), or three: a left cap of 8 pixels,
// the middle and a right cap of 8 pixels (19 texels high), one set per state (the over set falls back to the normal
// one); the title (a SysString) centered
INT NCHeaderCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCHeaderCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    UTexture* Whole = NULL;
    UTexture* Caps[3] = { NULL, NULL, NULL };
    switch (State)
    {
    case 0:
        Whole = WholeNormalTex;
        for (INT i = 0; i < 3; i++)
            Caps[i] = NormalTex[i];
        break;
    case 1:
        Whole = WholePushedTex;
        for (INT i = 0; i < 3; i++)
            Caps[i] = PushedTex[i];
        break;
    case 2:
        Whole = WholeOverTex;
        if (!Whole)
            Whole = WholeNormalTex;
        for (INT i = 0; i < 3; i++)
        {
            Caps[i] = OverTex[i];
            if (!Caps[i])
                Caps[i] = NormalTex[i];
        }
        break;
    }
    if (Whole)
    {
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, 0, 0, 67, 15, Whole, 0xFF);
    }
    else
    {
        if (Caps[0])
            Device->DrawStretchedTex(0, 0, 8, (INT)Height, 0, 0, 8, 19, Caps[0], 0xFF);
        if (Caps[1])
            Device->DrawStretchedTex(8, 0, (INT)(Width - 16.0f), (INT)Height, 0, 0, 8, 19, Caps[1], 0xFF);
        if (Caps[2])
            Device->DrawStretchedTex((INT)(Width - 8.0f), 0, 8, (INT)Height, 0, 0, 8, 19, Caps[2], 0xFF);
    }
    if (TitleID >= 0)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *SysString(TitleID), 0, 0, 0, 0, 0, L2FONT_Normal);
        INT X = (INT)((DOUBLE)Width * 0.5 - (DOUBLE)(Size.cx / 2));
        INT Y = (INT)((DOUBLE)Height * 0.5 - (DOUBLE)(Size.cy / 2));
        Device->DrawText(X, Y + 1, 0xFFE6DCBE, *SysString(TitleID), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10022820, NCHeaderCtrl, OnPaint)

// 0x10021f50 (vslot 68) - pushed; the click sound plays wherever the button went down
INT NCHeaderCtrl::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (IsInWindow(LOWORD(lParam), HIWORD(lParam)))
        State = 1;
    PlayInterfaceSound(IsEnabled() ? SOUND_Click : SOUND_ClickDisabled);
    return 1;
}
NW_HOOK_METHOD(0x10021f50, NCHeaderCtrl, OnLButtonDown)

// 0x10021fb0 (vslot 69) - released over the header: the list sorts by this column; then the face of where the pointer is
INT NCHeaderCtrl::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (IsInWindow(X, Y) && bClickable && NotifyID != -1)
        NotifyParent(NotifyID);
    State = CallIsTopWindowAt(X, Y) ? 2 : 0;
    return 1;
}
NW_HOOK_METHOD(0x10021fb0, NCHeaderCtrl, OnLButtonUp)

// 0x10021e80 (vslot 67, guard) - the over face while the top window under the pointer (the pressed one with the left button
// down), taking the mouse (SetCapture, slot 58); elsewhere it lets go (59)
INT NCHeaderCtrl::OnMouseMoveBody(WPARAM wParam, LPARAM lParam)
{
    guard(NCHeaderCtrl::OnMouseMove);
    if (CallIsTopWindowAt(LOWORD(lParam), HIWORD(lParam)))
    {
        State = (wParam & MK_LBUTTON) ? 1 : 2;
        SetCapture();
    }
    else
    {
        State = 0;
        ReleaseCapture();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10021e80, NCHeaderCtrl, OnMouseMoveBody)

// 0x100220a0 (anonymous in the db, ret 4) - the textures come by name; a missing one is the black texture (only the two
// whole-header names stay unset when empty); the over faces are the "<name>_over" textures (looked up without the black
// fallback: a missing one stays NULL and the normal face is drawn)
void NCHeaderCtrl::SetColumnInfo(NListColumnInfo* Info)
{
    TitleID = Info->TitleID;
    bClickable = Info->bClickable;
    bNumeric = Info->bNumeric;
    ColumnWidth = Info->Width;
    ColumnHeight = Info->Height;
    const TCHAR* Name = Info->WholeNormalTex;
    if (Name && *Name)
        WholeNormalTex = FindTexture(Name, 1);
    const TCHAR* PushedName = Info->WholePushedTex;
    if (PushedName && *PushedName)
        WholePushedTex = FindTexture(PushedName, 1);
    if (Name && *Name && !WholeOverTex)
    {
        FString OverName = FString::Printf(TEXT("%s_over"), Name);
        WholeOverTex = FindTexture(*OverName, 0);
    }
    for (INT i = 0; i < 3; i++)
    {
        const TCHAR* CapName = Info->NormalTex[i];
        if (CapName && *CapName)
        {
            NormalTex[i] = FindTexture(CapName, 1);
            FString OverName = FString::Printf(TEXT("%s_over"), CapName);
            OverTex[i] = FindTexture(*OverName, 0);
        }
        else
            NormalTex[i] = FindTexture(TEXT("NWindow.BlackTexture"), 1);
        const TCHAR* DownName = Info->PushedTex[i];
        if (DownName && *DownName)
            PushedTex[i] = FindTexture(DownName, 1);
        else
            PushedTex[i] = FindTexture(TEXT("NWindow.BlackTexture"), 1);
    }
}
NW_HOOK_METHOD(0x100220a0, NCHeaderCtrl, SetColumnInfo)

NW_CHECK_VTABLE(NCHeaderCtrl, 0x101a5df8, 75)

/*-----------------------------------------------------------------------------
    NCListCtrl.
-----------------------------------------------------------------------------*/

// 0x10023970 - NControl's constructor is inlined; the argument is bNoScrollBar; TextY starts at 3, the rows are 17 high
NCListCtrl::NCListCtrl(INT bInNoScrollBar)
{
    BackTex = NULL;
    SelectTex = NULL;
    ToolTipTex = NULL;
    TipTopTex = NULL;
    TipMidTex = NULL;
    TipBottomTex = NULL;
    ScrollUpButton = NULL;
    ScrollDownButton = NULL;
    ScrollBar = NULL;
    TopRow = 0;
    NumRows = 0;
    TextX = 0;
    bNoScrollBar = bInNoScrollBar;
    MaxRows = 100;
    VisibleRows = 10;
    MaxColumns = 3;
    LineGap = 1;
    Unknown238 = 16;
    Selected = -1;
    HoverRow = -1;
    TextY = 3;
    RowHeight = 17;
}
NW_HOOK_CTOR_ARGS(0x10023970, NCListCtrl, (INT A), (A))

// 0x10023b10 (ret 0x18) - the fourth argument is never read (the clan window passes 12); the sixth is the row height (C4)
NCListCtrl::NCListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused, INT bInNoScrollBar, INT InRowHeight)
{
    BackTex = NULL;
    SelectTex = NULL;
    ToolTipTex = NULL;
    TipTopTex = NULL;
    TipMidTex = NULL;
    TipBottomTex = NULL;
    MaxRows = InMaxRows;
    ScrollUpButton = NULL;
    ScrollDownButton = NULL;
    ScrollBar = NULL;
    TopRow = 0;
    NumRows = 0;
    TextX = 0;
    VisibleRows = InVisibleRows;
    Selected = -1;
    HoverRow = -1;
    bNoScrollBar = bInNoScrollBar;
    MaxColumns = InMaxColumns;
    LineGap = 1;
    Unknown238 = 15;
    TextY = 3;
    RowHeight = InRowHeight;
}
NW_HOOK_CTOR_ARGS(0x10023b10, NCListCtrl, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x10023aa0
NW_HOOK_DTOR(0x10023aa0, NCListCtrl)

// 0x10022260 (vslot 25) - the headers, the scroll buttons and the slider report here
INT NCListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_LISTCTRL_HEADER)
            return SortByColumn((INT)lParam) != 0;
        if ((INT)wParam == NID_LIST_SCROLLUP)
            return ScrollUp() != 0;
        if ((INT)wParam == NID_LIST_SCROLLDOWN)
            return ScrollDown() != 0;
        if ((INT)wParam == NID_LIST_SCROLLBAR)
            return SetTopRow((INT)lParam) != 0;
    }
    else if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10022260, NCListCtrl, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10022340 (vslot 37) - the scroll controls wait for the first column (AddColumn). The stock code stores each
// texture one call late; the names here are in the order of the calls
INT NCListCtrl::OnCreate()
{
    NControl::OnCreate();
    ToolTipTex = FindTexture(TEXT("L2UI.tooltip.mini-tool01"));
    TipTopTex = FindTexture(TEXT("L2UI.Tooltip.Tool02"));
    TipMidTex = FindTexture(TEXT("L2UI.Tooltip.Tool01"));
    TipBottomTex = FindTexture(TEXT("L2UI.Tooltip.Tool03"));
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    LineTex = FindTexture(TEXT("L2UI_CH3.Etc.textbackline"));
    return 1;
}
NW_HOOK_METHOD(0x10022340, NCListCtrl, OnCreate)

// 0x10023c10 (vslot 38) - rows (destructor and GMalloc free), columns (children, deleted), the arrays, the scroll
// controls
INT NCListCtrl::OnDestroy()
{
    guard(NCListCtrl::OnDestroy);
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows(i);
        if (Row)
        {
            Row->~NListRow();
            GMalloc->Free(Row);
        }
    }
    for (INT i = 0; i < Columns.Num(); i++)
    {
        NCHeaderCtrl* Column = Columns(i);
        if (Column)
        {
            RemoveChildWindow(Column->Entry);
            delete Column;
        }
    }
    Rows.Empty();
    Columns.Empty();
    if (ScrollUpButton)
    {
        RemoveChildWindow(ScrollUpButton->Entry);
        delete ScrollUpButton;
        ScrollUpButton = NULL;
    }
    if (ScrollDownButton)
    {
        RemoveChildWindow(ScrollDownButton->Entry);
        delete ScrollDownButton;
        ScrollDownButton = NULL;
    }
    if (ScrollBar)
    {
        RemoveChildWindow(ScrollBar->Entry);
        delete ScrollBar;
        ScrollBar = NULL;
    }
    Selected = -1;
    HoverRow = -1;
    TopRow = 0;
    NumRows = 0;
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10023c10, NCListCtrl, OnDestroy)

// 0x10022ad0 (vslot 62) - the background texture, a band under every second row line, a two-part bar under the selected
// row; each cell is clipped to its column (15 high): its pictures, its icon centered in the column and in the row, its
// text 10 pixels after the icon and the pictures. The rows are RowHeight (17) high
INT NCListCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCListCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    const INT RowH = RowHeight;
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    for (INT i = 0; i < VisibleRows + 1; i++)
    {
        if (i % 2 == 1)
            Device->DrawTexture(0, HeaderHeight + i * RowH - 1, (INT)Width, RowH, 0, 0, 8, 15, LineTex, 0xFF, 1);
    }
    INT Line = 0;
    for (INT i = TopRow; i < TopRow + VisibleRows && i <= NumRows && i < Rows.Num(); i++, Line++)
    {
        Device->SetClip(ScreenX, ScreenY + HeaderHeight, (INT)Width, (INT)(Height - (FLOAT)HeaderHeight));
        if (i == Selected)
        {
            Device->DrawStretchedTex(1, Line * RowH + HeaderHeight - 1, (INT)(Width - 47.0f - 2.0f), RowH, 0, 0, 16, 13,
                                     SelectTex, 0xFF);
            Device->DrawStretchedTex((INT)(Width + 1.0f - 47.0f - 2.0f), Line * RowH + HeaderHeight - 1, 32, RowH, 0, 0,
                                     32, 13, SelectTex2, 0xFF);
        }
        Device->ReleaseClip();
        INT X = TextX;
        NListRow* Row = Rows(i);
        for (INT c = 0; c < Columns.Num(); c++)
        {
            NCHeaderCtrl* Column = Columns(c);
            INT Y = TextY + Line * RowH + HeaderHeight;
            Device->SetClip(ScreenX + X, ScreenY + Y - 1, Column->ColumnWidth, 15);
            NListCell* Cell = (NListCell*)((BYTE*)Row->Cells + c * sizeof(NListCell));
            INT ImagesW = 0;
            for (INT k = 0; k < 16; k++)
            {
                NListCellImage* Image = &Cell->Images[k];
                if (Image->Tex)
                {
                    Device->DrawTexture(Image->X + X, Image->Y + Y, Image->W, Image->H, Image->U, Image->V,
                                        Image->UL ? Image->UL : Image->W, Image->VL ? Image->VL : Image->W, Image->Tex,
                                        0xFF, 1);
                    ImagesW += Image->W;
                }
            }
            if (Cell->Icon)
                Device->DrawTexture(Column->ColumnWidth / 2 - Cell->IconW / 2 + X,
                                    RowH / 2 - Cell->IconH / 2 + Line * RowH + HeaderHeight, Cell->IconW, Cell->IconH, 0,
                                    0, Cell->IconW, Cell->IconH, Cell->Icon, 0xFF, 1);
            if (Cell->bTextVisible)
                Device->DrawText(Cell->IconW + ImagesW + X + 10, Y, Cell->Color, Cell->Text, 0, 0, 0, L2FONT_Normal, 0,
                                 0, 0, 0, 0, 0, 0);
            X += Column->ColumnWidth;
            Device->ReleaseClip();
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10022ad0, NCListCtrl, OnPaint)

// 0x10023dd0 (vslot 63) - the selected row's tool tip is the window skin's full box, at the row's line
INT NCListCtrl::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (HoverRow >= 0 && HoverRow < MaxRows && HoverRow == Selected)
    {
        NListRow* Row = Rows(Selected);
        INT OffsetY = (HoverRow - TopRow) % VisibleRows * RowHeight + TextY + HeaderHeight;
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            ToolTipSkin->DrawFull(Device, Row->ToolTip, 0, OffsetY, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10023dd0, NCListCtrl, OnDrawToolTip)

// 0x100224e0 (vslot 67) - the row is asked through vslot 70 (C3)
INT NCListCtrl::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    HoverRow = CallHitTest(LOWORD(lParam), HIWORD(lParam));
    return 1;
}
NW_HOOK_METHOD(0x100224e0, NCListCtrl, OnMouseMove)

// 0x100224a0 (vslot 68) - selects without telling the owner (the stock code still tests the row against
// Rows.Num(), with nothing depending on it)
INT NCListCtrl::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT Row = CallHitTest(LOWORD(lParam), HIWORD(lParam));
    Selected = Row;
    HoverRow = Row;
    if (Row >= 0)
        (void)Rows.Num();
    return 1;
}
NW_HOOK_METHOD(0x100224a0, NCListCtrl, OnLButtonDown)

// 0x10022560 (vslot 73) - one row per wheel notch (120); the slider is optional now
INT NCListCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Notches = (SWORD)((SWORD)HIWORD(wParam) / 120);
    INT Delta = Notches;
    if (Notches > 0)
    {
        if (TopRow > Delta)
            TopRow -= Delta;
        else
            TopRow = 0;
    }
    else if (NumRows >= VisibleRows)
    {
        if (NumRows - TopRow + Delta > VisibleRows)
            TopRow -= Delta;
        else
            TopRow = NumRows - VisibleRows;
    }
    else
    {
        if (NumRows - TopRow + Delta > VisibleRows)
            TopRow -= Delta;
        else
            TopRow = 0;
    }
    if (ScrollBar)
        ScrollBar->SetValue(TopRow);
    return 1;
}
NW_HOOK_METHOD(0x10022560, NCListCtrl, OnMouseWheel)

// 0x10023780 (vslot 75) - rows (destructor and GMalloc free) and columns (children, deleted) go; the scroll controls stay
void NCListCtrl::ClearAll()
{
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows(i);
        if (Row)
        {
            Row->~NListRow();
            GMalloc->Free(Row);
        }
    }
    for (INT i = 0; i < Columns.Num(); i++)
    {
        NCHeaderCtrl* Column = Columns(i);
        if (Column)
        {
            RemoveChildWindow(Column->Entry);
            delete Column;
        }
    }
    Rows.Empty();
    Columns.Empty();
    if (ScrollBar)
        ScrollBar->Reset();
    TopRow = 0;
    NumRows = 0;
    Selected = -1;
    HoverRow = -1;
}
NW_HOOK_METHOD(0x10023780, NCListCtrl, ClearAll)

// 0x10022510 (vslot 77) - the owner gets the row (NWM_COMMAND, lParam = index)
INT NCListCtrl::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    INT Row = CallHitTest(LOWORD(lParam), HIWORD(lParam));
    Selected = Row;
    HoverRow = Row;
    if (Row >= 0 && Row < Rows.Num())
        NotifyParent(Row);
    return 1;
}
NW_HOOK_METHOD(0x10022510, NCListCtrl, OnLButtonDblClick)

// 0x10022620 (vslot 78)
INT NCListCtrl::ScrollUp()
{
    if (NumRows > VisibleRows)
    {
        if (--TopRow < 0)
            TopRow = 0;
        if (ScrollBar)
            ScrollBar->SetValue(TopRow);
    }
    return 1;
}
NW_HOOK_METHOD(0x10022620, NCListCtrl, ScrollUp)

// 0x10022660 (vslot 79)
INT NCListCtrl::ScrollDown()
{
    if (NumRows > VisibleRows)
    {
        INT Last = NumRows - VisibleRows;
        if (++TopRow > Last)
            TopRow = Last;
        if (ScrollBar)
            ScrollBar->SetValue(TopRow);
    }
    return 1;
}
NW_HOOK_METHOD(0x10022660, NCListCtrl, ScrollDown)

// 0x100226b0 (vslot 80) - from the slider
INT NCListCtrl::SetTopRow(INT Row)
{
    TopRow = Row;
    return 1;
}
NW_HOOK_METHOD(0x100226b0, NCListCtrl, SetTopRow)

// 0x10023000 (vslot 81) - bubble sort on the column's text (or value). The selection follows its row
INT NCListCtrl::SortByColumn(INT Column)
{
    NListRow* SelRow = NULL;
    if (Selected >= 0 && Selected < Rows.Num())
        SelRow = Rows(Selected);
    for (INT Pass = 0; Pass < Rows.Num(); Pass++)
    {
        for (INT i = 0; i < Rows.Num() - Pass && i < Rows.Num() - 1; i++)
        {
            NCHeaderCtrl* Header = Columns(Column);
            const TCHAR* A = Rows(i)->Cells[Column].Text;
            const TCHAR* B = Rows(i + 1)->Cells[Column].Text;
            UBOOL bSwap;
            if (Header->bNumeric)
            {
                INT ValueB = NWtol(B);
                INT ValueA = NWtol(A);
                if (Header->bAscending)
                    bSwap = ValueA > ValueB;
                else
                    bSwap = ValueA < ValueB;
            }
            else
            {
                if (Header->bAscending)
                    bSwap = _wcsicmp(A, B) > 0;
                else
                    bSwap = _wcsicmp(A, B) < 0;
            }
            if (bSwap)
            {
                NListRow* Temp = Rows(i);
                Rows(i) = Rows(i + 1);
                Rows(i + 1) = Temp;
            }
        }
    }
    NCHeaderCtrl* Sorted = Columns(Column);
    Sorted->bAscending = (Sorted->bAscending == 0);
    if (SelRow)
    {
        Selected = 0;
        for (; Selected < Rows.Num(); Selected++)
        {
            if (Rows(Selected) == SelRow)
                break;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x10023000, NCListCtrl, SortByColumn)

// 0x100223e0 (vslot 76) - the row under a screen point, among the lines shown (RowHeight high); -1 outside the rows
INT NCListCtrl::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    INT Result = -1;
    if (RX < 0)
        return -1;
    INT W = (INT)Width;
    if (RX > W)
        return -1;
    INT Top = HeaderHeight;
    if (RY < Top)
        return -1;
    if (RY > (INT)Height)
        return -1;
    INT Last = TopRow + VisibleRows;
    for (INT i = TopRow; i < Last && i < NumRows; i++, Top += RowHeight)
    {
        if (RX < W && RY >= Top && RY < Top + RowHeight)
        {
            Result = i;
            break;
        }
    }
    return Result;
}
NW_HOOK_METHOD(0x100223e0, NCListCtrl, HitTest)

// 0x100226f0
TArray<NListRow*>* NCListCtrl::GetRows()
{
    return &Rows;
}
NW_HOOK_METHOD(0x100226f0, NCListCtrl, GetRows)

// 0x10022700
INT NCListCtrl::GetRowCount()
{
    return Rows.Num();
}
NW_HOOK_METHOD(0x10022700, NCListCtrl, GetRowCount)

// 0x10022710
void NCListCtrl::SetSelected(INT Index)
{
    if (Index >= 0 && Index < Rows.Num())
        Selected = Index;
}
NW_HOOK_METHOD(0x10022710, NCListCtrl, SetSelected)

// 0x10022730
void NCListCtrl::SetTextOffset(INT X, INT Y)
{
    TextX = X;
    TextY = Y;
}
NW_HOOK_METHOD(0x10022730, NCListCtrl, SetTextOffset)

// 0x10022750 - C3: the slider is optional
void NCListCtrl::UpdateScrollRange()
{
    NumRows = Rows.Num();
    if (ScrollBar)
    {
        ScrollBar->SetRange(Rows.Num(), VisibleRows);
        ScrollBar->SetValue(TopRow);
    }
}
NW_HOOK_METHOD(0x10022750, NCListCtrl, UpdateScrollRange)

// 0x10022790 (vslot 48) - NCWnd shows it, then the scroll range is made again (the stock tail-jumps to UpdateScrollRange)
void NCListCtrl::ShowWindow()
{
    NCWnd::ShowWindow();
    UpdateScrollRange();
}
NW_HOOK_METHOD(0x10022790, NCListCtrl, ShowWindow)

// 0x10022f70
NListRow* NCListCtrl::GetSelectedRow()
{
    if (Selected >= 0 && Selected < Rows.Num())
        return Rows(Selected);
    return NULL;
}
NW_HOOK_METHOD(0x10022f70, NCListCtrl, GetSelectedRow)

// 0x10022f90 - the scroll buttons are created without auto-repeat, so this has no effect
void NCListCtrl::SetRepeatDelay(INT Delay)
{
    if (ScrollUpButton && ScrollDownButton)
    {
        ScrollUpButton->RepeatDelay = Delay;
        ScrollDownButton->RepeatDelay = Delay;
    }
}
NW_HOOK_METHOD(0x10022f90, NCListCtrl, SetRepeatDelay)

// 0x10022fc0 (name ours) - the range uses TopRow as the total (0 at creation), as in the stock code
void NCListCtrl::ResetScrollBar()
{
    SetRepeatDelay(200);
    if (ScrollBar)
    {
        ScrollBar->SetRange(TopRow, VisibleRows);
        ScrollBar->SetValue(TopRow);
    }
}
NW_HOOK_METHOD(0x10022fc0, NCListCtrl, ResetScrollBar)

// 0x10023200 - full lists refuse the row (the caller still owns it then)
INT NCListCtrl::AddRow(NListRow* Row)
{
    if (MaxRows <= Rows.Num())
        return 0;
    if (Row)
        Rows.AddItem(Row);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x10023200, NCListCtrl, AddRow)

// 0x100236f0 (name ours) - the columns stay
void NCListCtrl::ClearRows()
{
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows(i);
        if (Row)
        {
            Row->~NListRow();
            GMalloc->Free(Row);
        }
    }
    Rows.Empty();
    if (ScrollBar)
        ScrollBar->Reset();
    TopRow = 0;
    NumRows = 0;
    Selected = -1;
    HoverRow = -1;
}
NW_HOOK_METHOD(0x100236f0, NCListCtrl, ClearRows)

// 0x100226d0 (anonymous in the db, ret 4; name ours) - the number of columns the list takes, settable until the first one is added
void NCListCtrl::SetMaxColumns(INT InMaxColumns)
{
    if (Columns.Num() == 0)
        MaxColumns = InMaxColumns;
}
NW_HOOK_METHOD(0x100226d0, NCListCtrl, SetMaxColumns)

// 0x10023250 (guard name InitScrollBar) - up / down buttons and the slider along the right edge, under the first
// header; none when the list was made with bNoScrollBar
void NCListCtrl::InitScrollBar()
{
    guard(NCListCtrl::InitScrollBar);
    if (!(bNoScrollBar & 1))
    {
        NRectF Rect;
        Rect.X = 0;
        Rect.Y = 0;
        Rect.W = 0.0f;
        Rect.H = 0.0f;
        GetLocalRect(&Rect);
        INT HeaderH = 0;
        if (Columns.Num() > 0)
        {
            NRectF ColRect;
            ColRect.X = 0;
            ColRect.Y = 0;
            ColRect.W = 0.0f;
            ColRect.H = 0.0f;
            Columns(0)->GetLocalRect(&ColRect);
            HeaderH = (INT)ColRect.H;
        }

        ScrollUpButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"),
                                          TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"));
        ScrollUpButton->ID = NID_LIST_SCROLLUP;
        ScrollUpButton->SetPosition((INT)(Rect.W - 15.0f), HeaderH, 15, 15);
        CreateChild(ScrollUpButton, NWS_VISIBLE);

        ScrollDownButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"),
                                            TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"));
        ScrollDownButton->ID = NID_LIST_SCROLLDOWN;
        ScrollDownButton->SetPosition((INT)(Rect.W - 15.0f), (INT)(Rect.H - 15.0f), 15, 15);
        CreateChild(ScrollDownButton, NWS_VISIBLE);

        ScrollBar = new NCSliderBar(TEXT("L2UI_CH3.ScrollBar.SliderBarTop"), TEXT("L2UI_CH3.ScrollBar.SliderBarCenter"),
                                    TEXT("L2UI_CH3.ScrollBar.SliderBarBottom"));
        ScrollBar->ID = NID_LIST_SCROLLBAR;
        ScrollBar->SetPosition((INT)(Rect.W - 15.0f), HeaderH + 15, 15, (INT)(Rect.H - 30.0f - (FLOAT)HeaderH));
        CreateChild(ScrollBar, NWS_VISIBLE);

        ResetScrollBar();
    }
    unguard;
}
NW_HOOK_METHOD(0x10023250, NCListCtrl, InitScrollBar)

// 0x10023ee0 (name ours) - a header after the existing ones (Index is only checked against MaxColumns: the owners
// pass 0 .. 3); the rows start under the last header added
INT NCListCtrl::AddColumn(INT Index, NListColumnInfo* Info)
{
    if (Index > MaxColumns)
        return 0;
    NCHeaderCtrl* Header = new NCHeaderCtrl;
    Header->SetColumnInfo(Info);
    Header->ID = NID_LISTCTRL_HEADER;
    INT X = 0;
    for (INT i = 0; i < Columns.Num(); i++)
        X += Columns(i)->ColumnWidth;
    Header->SetPosition(X, 0, Info->Width, Info->Height);
    CreateChild(Header, NWS_VISIBLE);
    Header->NotifyID = Columns.Num();
    Columns.AddItem(Header);
    if (!ScrollBar)
        InitScrollBar();
    UpdateScrollRange();
    HeaderHeight = Info->Height;
    return 1;
}
NW_HOOK_METHOD(0x10023ee0, NCListCtrl, AddColumn)

NW_CHECK_VTABLE(NCListCtrl, 0x101a5f28, 82)
