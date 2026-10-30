#pragma once
// NCListCtrl - list with columns (UClass "CListCtrl", 0x24C): a header button per column
// (NCHeaderCtrl; a click sorts the rows by that column, the next click reverses the order),
// rows of cells under them (text, color, optional icon), scroll buttons and a slider on the
// right. The owner builds each row (NListRow, one NListCell per column) and hands it over
// with AddRow; from then on the list owns it. A double click reports the row (NWM_COMMAND,
// lParam = index); the selected row shows its tool tip while the mouse is over it.
// The server list of the login screen, the clan members, party matching and the siege
// lists derive from it (NCServerSelectListCtrl, NCMatchListCtrl, NCSiegeInfoListCtrl).
//
// C3 (read from the stock code, 0x1001f3c0 .. 0x10021540): the rows are 17 pixels high and every second one has a
// band texture under it, the selected row a two-part bar, the cells' text starts 10 pixels after the icon, the
// headers are drawn from caps and the tool tip is the window skin's.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPushButton;
class NCSliderBar;

// NWM_COMMAND id of the header buttons (NotifyID = column index)
enum { NID_LISTCTRL_HEADER = 0xF5 };

// One of the pictures a cell can carry (name ours; 0x24 bytes, new in C4: the cell has sixteen of them and NCListCtrl::OnPaint
// draws the ones with a texture, at an offset in the column and the row; Icon / IconW / IconH below stay the one centered picture)
struct NListCellImage
{
    UTexture* Tex;             // +0x00 NULL = unused
    INT X;                     // +0x04 offset from the column's left edge (the text follows the pictures drawn)
    INT Y;                     // +0x08 offset from the row's top
    INT W;                     // +0x0C size on the screen
    INT H;                     // +0x10
    INT U;                     // +0x14 the part of the texture
    INT V;                     // +0x18
    INT UL;                    // +0x1C 0 = W
    INT VL;                    // +0x20 0 = W
};
NW_ASSERT_SIZE(NListCellImage, 0x24);

// A cell (name ours; 0x1A64 bytes in C4, 0x1824 in C3, 0x1820 in C2, 0xA1C in C1: the texts got longer, then the sixteen pictures).
// What the list draws: the pictures, Icon centered in the column, then Text after IconW + 10 pixels, in Color.
struct NListCell
{
    TCHAR Text[2048];          // +0x0000 (C1: 1024)
    UTexture* Icon;            // +0x1000
    NListCellImage Images[16]; // +0x1004 (C4)
    DWORD Color;               // +0x1244 (0xFFDCDCDC)
    TCHAR Text2[1024];         // +0x1248 not drawn by the list (the owners keep data here) (C1: 256)
    INT Param1;                // +0x1A48 (-1) for the owner
    INT Param2;                // +0x1A4C (-1)
    INT Param3;                // +0x1A50 new in C2 (SetParams' third argument; the constructor leaves it)
    INT UnknownA10;            // +0x1A54 (0)
    INT IconW;                 // +0x1A58
    INT IconH;                 // +0x1A5C
    UBOOL bTextVisible;        // +0x1A60 (name ours) new in C3: 1 in the constructor; OnPaint draws the text only while it is set

    // 0x100227a0 (anonymous in the db): out of line, called for each cell of a row
    __declspec(noinline) NListCell();
    void SetText(const TCHAR* InText);                   // 0x10021b10
    void SetNumber(INT Value);                           // 0x10021b40 "%d"
    void SetIcon(UTexture* InIcon, INT W, INT H);        // 0x10021b60 (NULL keeps the old one)
    void SetText2(const TCHAR* InText);                  // 0x10021c40
    void SetParams(INT InParam1, INT InParam2, INT InParam3 = 0); // 0x10021c70 (C1: two arguments)
    void SetColor(DWORD InColor);                        // 0x10021ca0
};
NW_ASSERT_OFFSET(NListCell, Icon, 0x1000);
NW_ASSERT_OFFSET(NListCell, Images, 0x1004);
NW_ASSERT_OFFSET(NListCell, Color, 0x1244);
NW_ASSERT_OFFSET(NListCell, Text2, 0x1248);
NW_ASSERT_OFFSET(NListCell, Param1, 0x1A48);
NW_ASSERT_OFFSET(NListCell, IconW, 0x1A58);
NW_ASSERT_OFFSET(NListCell, bTextVisible, 0x1A60);
NW_ASSERT_SIZE(NListCell, 0x1A64);

// A row (name ours; 0x890 bytes in C4, 0x88C in C3, created by the owner with new).
struct NListRow
{
    NListCell* Cells;          // +0x000 one per column
    TCHAR Text[1024];          // +0x004 not drawn by the list
    INT Param1;                // +0x804 (-1) for the owner
    INT Param2;                // +0x808 (-1)
    INT Param3;                // +0x80C new in C2 (SetParams' third argument)
    NToolTipInfo ToolTip;      // +0x810 shown while the selected row is under the mouse (C1: TCHAR[1024]); the
                               //   constructor / destructor are NToolTipInfo's (0x10002d90 / 0x10002e40)

    NListRow(INT NumCells);                              // 0x10023610 (ret 4)
    __declspec(noinline) ~NListRow();                    // 0x100236c0 (frees Cells, then the tool tip)
    void SetText(const TCHAR* InText);                   // 0x10021e00
    void SetParams(INT InParam1, INT InParam2, INT InParam3 = 0); // 0x10021e30 (C1: two arguments)
    const TCHAR* GetToolTip();                           // C2: no stock function (C1: NULL when the text is empty)
};
NW_ASSERT_OFFSET(NListRow, Text, 0x4);
NW_ASSERT_OFFSET(NListRow, Param1, 0x804);
NW_ASSERT_OFFSET(NListRow, ToolTip, 0x810);
NW_ASSERT_SIZE(NListRow, 0x890);

// What a column is made of (name ours; the db calls it NCLVColumn; 0x4018 bytes in C4 (0x4814 in C3, 0x1814 in C2), built on the owner's
// stack and handed to NCListCtrl::AddColumn). The header is drawn from three textures per state (left cap, middle, right cap):
// the three normal names ("up"), then the three pushed ones ("down"; the over face is "<name>_over"); the two last strings name
// one texture each for a header drawn whole. C4: the title is a SysString index now.
struct NListColumnInfo
{
    INT Width;                 // +0x000
    INT Height;                // +0x004 of the header; the rows start under it
    INT TitleID;               // +0x008 (name ours) SysString index of the title (-1 = none)
    UBOOL bClickable;          // +0x00C (1) a click on the header sorts
    UBOOL bNumeric;            // +0x010 (0) sort by value (_wtol) instead of by text
    INT Unknown14;             // +0x014 (left as it was on the stack)
    TCHAR NormalTex[3][1024];  // +0x018, +0x818, +0x1018 the normal state: left cap, middle, right cap
                               //   ("NWindow.BlackTexture" in the header when empty)
    TCHAR PushedTex[3][1024];  // +0x1818, +0x2018, +0x2818 the pushed state (SetTabDownTex)
    TCHAR WholeNormalTex[1024];  // +0x3018 (names ours) a texture for the whole normal header (67 x 15 texels), none
                                 //   when empty
    TCHAR WholePushedTex[1024];  // +0x3818 the same for the pushed state

    // inlined in the owners (the first character of all eight strings is cleared)
    NListColumnInfo()
    {
        TitleID = -1;
        bClickable = 1;
        bNumeric = 0;
        NormalTex[0][0] = 0;
        PushedTex[0][0] = 0;
        NormalTex[1][0] = 0;
        PushedTex[1][0] = 0;
        NormalTex[2][0] = 0;
        PushedTex[2][0] = 0;
        WholeNormalTex[0] = 0;
        WholePushedTex[0] = 0;
    }
    void SetTitleID(INT InTitleID);                      // 0x10005ba0 (anonymous in the db; ret 4)
    // Index 0, 1 or 2 picks the cap (any other Index is ignored)
    void SetNormalTex(const TCHAR* Name, INT Index = 0); // 0x10021cb0 (ret 8)
    void SetPushedTex(const TCHAR* Name, INT Index = 0); // 0x10021d40 (ret 8; the guard says NCLVColumn::SetTabDownTex; 0..2 only)
};
NW_ASSERT_OFFSET(NListColumnInfo, TitleID, 0x8);
NW_ASSERT_OFFSET(NListColumnInfo, bClickable, 0xC);
NW_ASSERT_OFFSET(NListColumnInfo, NormalTex, 0x18);
NW_ASSERT_OFFSET(NListColumnInfo, PushedTex, 0x1818);
NW_ASSERT_OFFSET(NListColumnInfo, WholeNormalTex, 0x3018);
NW_ASSERT_OFFSET(NListColumnInfo, WholePushedTex, 0x3818);
NW_ASSERT_SIZE(NListColumnInfo, 0x4018);

// NCHeaderCtrl - a column's header button (UClass "CHeaderCtrl", 0x240): drawn from textures (a whole one, or a left cap, a
// middle and a right cap per state: normal, pushed, mouse over), the title (a SysString) centered. Released over it, it tells
// the list (NID_LISTCTRL_HEADER, lParam = NotifyID = column index). C4: the State 0 / 1 / 2 replaces bReleased, the pointer
// over the header takes the mouse (SetCapture) and shows the over face.
class NCHeaderCtrl : public NControl
{
    NW_DECLARE_CLASS(NCHeaderCtrl, NControl, 0, 0x102C2A50)

    UTexture* WholeNormalTex;        // +0x1F4 (names ours) a texture for the whole header, 67 x 15 texels (0 = the caps)
    UTexture* WholePushedTex;        // +0x1F8
    UTexture* WholeOverTex;          // +0x1FC the over face of the whole texture ("<name>_over"; 0 = the normal one)
    UTexture* NormalTex[3];          // +0x200 left cap, middle, right cap ("NWindow.BlackTexture" by default)
    UTexture* PushedTex[3];          // +0x20C the same while pushed
    UTexture* OverTex[3];            // +0x218 the same with the mouse over (0 = the normal cap)
    INT TitleID;                     // +0x224 (name ours) SysString index of the title; the constructor leaves it as allocated
    union
    {
        INT State;                   // +0x228 (name ours) 0 normal, 1 pushed, 2 mouse over
        UBOOL bReleased;             // the C3 name (1 there = normal)
    };
    UBOOL bClickable;                // +0x22C (1)
    UBOOL bNumeric;                  // +0x230 (0) the column sorts by value
    UBOOL bAscending;                // +0x234 (1) order of the next sort; each sort flips it
    INT ColumnWidth;                 // +0x238
    INT ColumnHeight;                // +0x23C

    NCHeaderCtrl();                                                  // 0x10023860
    // ~NCHeaderCtrl                                                 // 0x10023920

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (folded)
    virtual INT OnDestroy();                                         // 38 0x10022020
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10022820
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10021f50
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10021fb0

    void SetColumnInfo(NListColumnInfo* Info);                       // 0x100220a0 (anonymous in the db, ret 4, name ours)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);  // 67 0x10021e80 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    // 0x10021e80: the body of slot 67, hooked by its address under this name (the slot's own definition is the generated stub)
    INT OnMouseMoveBody(WPARAM wParam, LPARAM lParam);
};
NW_ASSERT_OFFSET(NCHeaderCtrl, NormalTex, 0x200);
NW_ASSERT_OFFSET(NCHeaderCtrl, OverTex, 0x218);
NW_ASSERT_OFFSET(NCHeaderCtrl, TitleID, 0x224);
NW_ASSERT_OFFSET(NCHeaderCtrl, State, 0x228);
NW_ASSERT_OFFSET(NCHeaderCtrl, ColumnWidth, 0x238);
NW_ASSERT_SIZE(NCHeaderCtrl, 0x240);

class NCListCtrl : public NControl
{
    NW_DECLARE_CLASS(NCListCtrl, NControl, 0, 0x102C2F80)

    UBOOL bNoScrollBar;              // +0x1F4 (name ours) the constructors' last argument (0): when set the list makes
                                     //   no scroll buttons and no slider (InitScrollBar tests bit 0)
    UTexture* BackTex;               // +0x1F8 set by the owner, drawn under the rows
    UTexture* SelectTex;             // +0x1FC "L2UI_CH3.ListCtrl.TextSelect" the selected row's bar (left part)
    UTexture* SelectTex2;            // +0x200 "L2UI_CH3.ListCtrl.TextSelect2" (its right end)
    UTexture* ToolTipTex;            // +0x204 "L2UI.tooltip.mini-tool01" (not drawn here)
    UTexture* TipTopTex;             // +0x208 "L2UI.Tooltip.Tool02" the C2 row tool tip: top,
    UTexture* TipMidTex;             // +0x20C "L2UI.Tooltip.Tool01"  a strip per text line,
    UTexture* TipBottomTex;          // +0x210 "L2UI.Tooltip.Tool03"  bottom (C3 draws it through the window's skin)
    UTexture* LineTex;               // +0x214 "L2UI_CH3.Etc.textbackline" the band under every second row
    TArray<NListRow*> Rows;          // +0x218
    TArray<NCHeaderCtrl*> Columns;   // +0x224
    NCPushButton* ScrollUpButton;    // +0x230 the scroll controls come with the first column
    NCPushButton* ScrollDownButton;  // +0x234
    NCSliderBar* ScrollBar;          // +0x238
    INT VisibleRows;                 // +0x23C (10)
    INT MaxRows;                     // +0x240 (100) AddRow refuses more
    INT MaxColumns;                  // +0x244 (3)
    INT TopRow;                      // +0x248 first row shown
    INT NumRows;                     // +0x24C rows when the scroll range was last set
    INT Selected;                    // +0x250 -1 = none
    INT HoverRow;                    // +0x254 row under the mouse, -1 = none
    INT Unknown238;                  // +0x258 (16; 15 from the sized constructor), never read
    INT HeaderHeight;                // +0x25C the rows start under the headers (AddColumn)
    INT TextX;                       // +0x260 where the cells start
    INT TextY;                       // +0x264
    INT LineGap;                     // +0x268 (1) not read by C3's code (the rows are 17 high)
    INT RowHeight;                   // +0x26C (name ours) new in C4: 17, the constructors store it (OnPaint, HitTest)

    NCListCtrl(INT bInNoScrollBar = 0);                              // 0x10023970 (ret 4)
    NCListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused,
               INT bInNoScrollBar = 0, INT InRowHeight = 17);        // 0x10023b10 (ret 0x18; the stock subclasses pass 0, 0x11)
    // ~NCListCtrl                                                   // 0x10023aa0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10022260
    virtual INT OnCreate();                                          // 37 0x10022340
    virtual INT OnDestroy();                                         // 38 0x10023c10
    virtual void ShowWindow();                                       // 48 0x10022790
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10022ad0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10023dd0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100224e0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100224a0
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x10022560
    // 68 is gone (C3); 69 and 70 are placeholders defined in NNewVirtuals.gen.cpp: their stock bodies are
    // ClearAll (0x10020d20) and HitTest (0x1001fb00) below, hooked by address
    virtual void NewVirtual75();  // 75 0x10023780 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual76();  // 76 0x100223e0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 77 0x10022510
    virtual INT ScrollUp();                                          // 78 0x10022620
    virtual INT ScrollDown();                                        // 79 0x10022660
    virtual INT SetTopRow(INT Row);                                  // 80 0x100226b0
    virtual INT SortByColumn(INT Column);                            // 81 0x10023000
    // text of the row tool tip, drawn at (X, Y) in its box (the subclasses draw it)
    void DrawRowToolTip(NCGDevice* Device, NListRow* Row, INT X, INT Y) {} // C2: slot gone (was 69) (folded)

    // names ours
    INT HitTest(INT X, INT Y);                                       // 0x100223e0 (vslot 70) row, -1 = none
    INT CallHitTest(INT X, INT Y) { return ::nw::VirtualCall<INT>(this, 76, X, Y); }
    void ClearAll();                                                 // 0x10023780 (vslot 69) rows and columns go
    TArray<NListRow*>* GetRows();                                    // 0x100226f0
    INT GetRowCount();                                               // 0x10022700
    void SetSelected(INT Index);                                     // 0x10022710
    void SetTextOffset(INT X, INT Y);                                // 0x10022730
    __declspec(noinline) void UpdateScrollRange();                   // 0x10022750 (ShowWindow tail-jumps to it)
    NListRow* GetSelectedRow();                                      // 0x10022f70
    __declspec(noinline) void SetRepeatDelay(INT Delay);                                  // 0x10022f90
    void ResetScrollBar();                                           // 0x10022fc0 (name ours) delay 200, range, value
    INT AddRow(NListRow* Row);                                       // 0x10023200
    void ClearRows();                                                // 0x100236f0 (name ours) rows go, columns stay
    void SetMaxColumns(INT InMaxColumns);                            // 0x100226d0 (anonymous in the db, name ours) only while there is no column
    __declspec(noinline) void InitScrollBar();                                            // 0x10023250 (guard name) buttons and slider
    INT AddColumn(INT Index, NListColumnInfo* Info);                 // 0x10023ee0 (name ours)
};
NW_ASSERT_OFFSET(NCListCtrl, BackTex, 0x1F8);
NW_ASSERT_OFFSET(NCListCtrl, Rows, 0x218);
NW_ASSERT_OFFSET(NCListCtrl, Columns, 0x224);
NW_ASSERT_OFFSET(NCListCtrl, ScrollBar, 0x238);
NW_ASSERT_OFFSET(NCListCtrl, TopRow, 0x248);
NW_ASSERT_OFFSET(NCListCtrl, Selected, 0x250);
NW_ASSERT_OFFSET(NCListCtrl, HeaderHeight, 0x25C);
NW_ASSERT_OFFSET(NCListCtrl, LineGap, 0x268);
NW_ASSERT_SIZE(NCListCtrl, 0x270);

#pragma pack(pop)
