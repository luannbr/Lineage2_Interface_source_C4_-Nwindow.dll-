#pragma once
// NCCmdTarget / NCWnd - base of every window and control (UClass "CCmdTarget", "CWnd").
//
// A window lives in a tree: NCVirtualWndMain::RegisterNWindow gives it an NWndEntry, its
// parent keeps the entries of its children in a linked list (NWndListNode), messages arrive
// through WindowProc (vslot 24) as Win32 message numbers. Coordinates: Local* are relative
// to the parent, Screen* absolute; SetPosition keeps both.

#include "nwindow/NCObject.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCWnd;
class NCVirtualWndMain;
class NCFrameCtrl;
class NCIcon;
class NCGDevice;

// Registration record of a window (NCVirtualWndMain::RegisterNWindow, 16 bytes) (name ours).
struct NWndEntry
{
    INT bValid;              // +0x00 1 while registered
    NCWnd* Wnd;              // +0x04
    NCVirtualWndMain* Main;  // +0x08 the manager that registered it
    NWndEntry* Parent;       // +0x0C entry of the parent window, NULL for top-level windows

    NWndEntry() : bValid(0), Wnd(NULL), Main(NULL), Parent(NULL) {}
};
NW_ASSERT_SIZE(NWndEntry, 0x10);

// Node of NCWnd's child list, allocated with new (12 bytes) (name ours).
struct NWndListNode
{
    NWndEntry* Entry;        // +0x00
    NWndListNode* Prev;      // +0x04
    NWndListNode* Next;      // +0x08

    NWndListNode() : Entry(NULL), Prev(NULL), Next(NULL) {}
};
NW_ASSERT_SIZE(NWndListNode, 0xC);

// NCWnd::Style bits (names ours)
enum
{
    NWS_VISIBLE = 0x0002,
    NWS_UNKNOWN20 = 0x0020,        // C2 (name ours): NCWnd::OnCreate sets it, with NWS_TOPMOST | NWS_VISIBLE
                                   //   (0x1022), on the icon of a minimizable window; its effect is not known
    NWS_NOHITTEST = 0x0008,        // skipped by NCVirtualWndMain hit tests
    NWS_NOTINPARENTLIST = 0x0040,  // RemoveChildWindow leaves the parent's list alone
    NWS_TITLEBAR = 0x0080,         // OnCreate adds an NCFrameCtrl title bar
    NWS_TOPMOST = 0x1000,          // NCVirtualWndMain keeps these in a separate array
    NWS_MINIMIZABLE = 0x2000,      // OnCreate adds an NCIcon shown while minimized; a
                                   //   decorated NCFrameCtrl adds a minimize button
    NWS_NOCLOSE = 0x4000,          // a decorated NCFrameCtrl leaves out its close button
    NWS_MODAL = 0x0100,            // C2 (name ours): SetModal / ClearModal (vslots 53, 54) set and clear it
    NWS_NOFOCUSNEXT = 0x80000,     // C3 (name ours): FocusNextWindow passes over it
    NWS_CLIPTOPARENT = 0x100000,   // C3 (name ours): SetPosition keeps the window inside its parent (ClipToParent)
    NWS_NOFOCUSSEARCH = 0x200000,  // C3 (name ours): SetFocus does not look below the window for a taker
    NWS_DRAGABLE = 0x400000,       // C3 (name ours): the mouse drags the window (OnLButtonDown / OnMouseMove)
    NWS_FIRSTCHILD = 0x800000,     // C4 (name ours): AddChildNode puts the child at the head of the list instead of the tail
    NWS_ICON = 0x81022,            // C3 (name ours): the style OnCreate gives the minimized icon
    NWS_ITEMSELECT = 0x40000,      // C2 (name ours): the inventory is picking an item (vslots 51, 52 set and
                                   //   clear it: NConsoleWnd::SelectItemToEnchant, NCInventoryWnd::OnMouseMove)
};

// Sent by NCVirtualWndMain after each paint to the window under the cursor, wParam = the
// NCGDevice, handled by vslot 51 (tool tips) (name ours).
enum { NWM_COMMAND = 0x501, NWM_DRAWTOOLTIP = 0x505 };

// NCWnd statics (names ours)
#if NW_HOOK_MODE
// C2 addresses (db/amap.csv: C1 0x10123d54 / 0x10123d50 / 0x1010a998; the generator left the C1 ones here)
#define GFocusEntry  NW_GLOBAL(NWndEntry*, 0x102C6BBC)  // entry of the window with the keyboard focus
#define GShiftDown   NW_GLOBAL(INT, 0x102C6BB8)         // Shift held (set by OnKeyDown/OnKeyUp)
#define GNextFrameID NW_GLOBAL(INT, 0x1022D9E0)         // next id for a title bar (NCWnd::OnCreate)
#else
extern NWndEntry* GFocusEntry;
extern INT GShiftDown;
extern INT GNextFrameID;
#endif

// The tool tips of the C2 windows (names ours). NCWnd::OnCreate makes an NToolTipSkin for every window (nine
// textures "L2UI.Tooltip.Tool01".."Tool09" and an NToolTipInfo). What a tool tip shows is an NToolTipInfo that
// its owner keeps (NCButton +0x1D0, NItemEntry +0x19B0, NListRow +0x810, a macro's first member): filled by SetText
// or one of the Fill* builders, which end by measuring it (Layout). The window's OnDrawToolTip hands the skin a
// copy of the info by value, the window's rectangle and the position of the thing the tip is about; the skin
// draws its box with the nine textures and the text on it: a small box with the title line (Draw, for the item
// under the mouse) or the full box with the lines and the description (DrawFull, for the selected item).
// Layouts from the stock constructor 0x10002230, copy constructor 0x10002560, copy assignment 0x1001aca0, the
// skin's destructor 0x10029f20, the builders and the draw routines (0x10027500 .. 0x1002a230).

class NConsoleWnd;
struct NItemEntry;
struct NListRow;

// NToolTipInfo::Style (+0x50): what the tool tip is of; the builders and the draw routines branch on it. The
// values are the ones the windows pass to NItemEntry::FillToolTip (names ours)
enum
{
    NTT_ItemName = 0x001,    // an item's name, additional name and grade, then its weight (a short tip)
    NTT_Item = 0x002,        // an item: the title, then a line per property of its type, the description
    NTT_Skill = 0x004,       // a skill: the title with its level, then type, MP cost and range
    NTT_Action = 0x008,      // an action (name only; Fill takes it with NTT_Name)
    NTT_SkillName = 0x010,   // a skill's name and level only (the abnormal status icons)
    NTT_ShortCut = 0x020,    // a shortcut slot (an item, a skill, an action or a macro: the title depends on Type)
    NTT_Recipe = 0x040,      // a recipe: name, additional name, grade, the two lines of its numbers
    NTT_Name = 0x080,        // the name only
    NTT_Text = 0x100,        // a name and a description (the macros: FillMacro)
    NTT_Desc = 0x200,        // a description under a few lines only (the party matching rows: FillMatchRow)
    NTT_Plain = 0x400,       // SetText: a text, the first line of the title
    NTT_Wanted = 0x800,      // with NTT_Item: the private store's buy list also shows the amount wanted
    NTT_Unknown1000 = 0x1000,  // C3 (name ours): Layout sizes only the title box, 174 wide, with the lines under it
    NTT_Unknown2000 = 0x2000,  // C3 (name ours): Layout treats it as NTT_Text / NTT_Plain (name, description)
    NTT_Unknown8000 = 0x8000,  // C3 (name ours): Layout treats it as NTT_Desc (lines and description, 174 wide)
};

// NToolTipInfo::Options (+0x54): the second argument of Fill (names ours)
enum
{
    NTTO_NoEnchant = 0x01,   // the "+N" before the name is not shown
    NTTO_NoCount = 0x02,     // the " (count)" after the name is not shown
    NTTO_Price = 0x04,       // Fill adds a first line "Price : N Adena" (the shops'); drawn under the name
    NTTO_PriceEach = 0x08,   // the same with the "For Each" before the number (a stack's unit price)
    NTTO_Count = 0x20,       // Fill adds a first line with the count alone (the shortcut slot's amount)
};

// One line of the tool tip (0x414 bytes; the lines array's element): "label : value". The lines are built on the
// stack and copied into the array with the stock `rep movsd` of 0x105 dwords (name ours)
struct NToolTipLine
{
    TCHAR Label[256];       // +0x000
    TCHAR Value[256];       // +0x200
    DWORD LabelColor;       // +0x400 grey (0xFFA3A3A3); the draw routines use the constant, not this
    DWORD ValueColor;       // +0x404 0xFFB09B79; an amount of adena takes the color of its size (NumberColor)
    INT Unused408;          // +0x408 never written by the builders (the stack's garbage is copied)
    INT bLabel;             // +0x40C 1: the label is drawn before the value; 0: the value alone
    INT bColon;             // +0x410 1: the label is followed by " : ", 0: by a space
    UTexture* Icon;         // +0x414 new in C3: the texture the NTT_Unknown1000 tips draw before the label (the lines
                            //   are copied with 0x106 dwords there)

    // as the builders initialize their local line (the Label/Value start empty)
    NToolTipLine() : LabelColor(0xFFA3A3A3), ValueColor(0xFFB09B79), Unused408(0), bLabel(1), bColon(1), Icon(NULL)
    {
        Label[0] = 0;
        Value[0] = 0;
    }
};
NW_ASSERT_SIZE(NToolTipLine, 0x418);
typedef TArray<NToolTipLine> NToolTipLineArray;

// a window's screen rectangle as the skin's draw routines take it by value: x, y (INT), width, height (FLOAT, as
// NCWnd's own rectangle: NRectF) (name ours); also the two boxes of NToolTipInfo, whose X and Y nothing uses
struct NToolTipRect
{
    INT X, Y;
    FLOAT W, H;
    // C4 (names ours; the tab control's hit tests): the point is inside (edges included); one of the corners of the other
    // rectangle (Other is passed by value) is inside
    __declspec(noinline) INT Contains(INT PX, INT PY);                   // 0x1002d6c0 (anonymous in the db, ret 8)
    __declspec(noinline) INT ContainsCornerOf(NToolTipRect Other);       // 0x1002d720 (anonymous in the db, ret 0x10)
};

struct NToolTipInfo
{
    FString Name;           // +0x00 the item's / skill's name
    FString Name2;          // +0x0C its additional name (shown after the name, in yellow)
    INT Unknown18;          // +0x18 new in C4 (name ours): the constructor stores -1; everything after it moved by 4
    INT Enchant;            // +0x1C the "+N" before the name (shown when > 0)
    INT Grade;              // +0x1C crystal grade (NConsoleWnd::GetGradeText: "`D`")
    INT Count;              // +0x20 the amount (the stack, or the trade's offer)
    INT Extra24;            // +0x24 a shortcut skill's second number: " (<sysstring 91>:N)" after its level (> 0)
    INT bShowCount;         // +0x28 the amount is shown (" (N)")
    INT Type;               // +0x2C the item's Type (NItemEntry +0x190C: 0 weapon, 1 armor, 2 jewel, 3.., 5 etc);
                            //   -1 none (C2: also the kind of a shortcut, now ShortCutType)
    INT Level;              // +0x30 a skill's level
    INT ItemID;             // +0x34 the item's id (NItemEntry +0x1918), only stored
    INT ShortCutType;       // +0x38 new in C3 (name ours): what a NTT_ShortCut tip is of: 1 item, 2 skill, 3 action,
                            //   4 macro (C2 kept this in Type; the constructor clears it, Clear leaves it alone)
    NToolTipLineArray Lines;     // +0x3C the property lines (elements of 0x418 bytes) (C2 +0x38)
    FString Description;    // +0x48 the item's / skill's description (the newlines escaped) (C2 +0x44)
    DWORD Style;            // +0x54 NTT_* (C2 +0x50)
    DWORD Options;          // +0x58 NTTO_* (C2 +0x54)
    NToolTipRect FullBox;   // +0x5C (X, Y unused) the full box's text size: W (+0x64) and H (+0x68), by Layout (C2 +0x58)
    NToolTipRect TitleBox;  // +0x6C (X, Y unused) the title line's size: W (+0x74) and H (+0x78), by Layout (C2 +0x68)

    // out of line like the stock ones. C4 addresses (the hooks of NToolTip.cpp; the numbers this block had up to stage 4 wave C were the C3
    // client's: SetText was 0x1002C4D0, which in C4 is NCSliderBar::SetScrollPos)
    __declspec(noinline) NToolTipInfo();                        // 0x10002d90
    __declspec(noinline) NToolTipInfo(const NToolTipInfo& Src); // 0x10002eb0 (the copy a by-value argument is made of)
    __declspec(noinline) ~NToolTipInfo();                       // 0x10002e40 (the members' destructors)
    __declspec(noinline) NToolTipInfo& operator=(const NToolTipInfo& Src);  // 0x1001fcc0

    // 0x1002ea90: the sizes of the boxes for the Style, from the texts and the font (the builders end with it)
    __declspec(noinline) void Layout();
    // 0x10030600: back to an empty tip (Name, Name2, Description, the lines and the numbers; Style / Options kept
    // by the builders' own stores)
    __declspec(noinline) void Clear();
    // 0x10032460 (ret 8): Clear, the text and its Style (NCButton::SetToolTip passes NTT_Plain), then Layout
    __declspec(noinline) void SetText(const TCHAR* InText, INT InStyle);
    // 0x10031ac0 (ret 0xC, name ours): a tip of a name and a description (either may be NULL, both: nothing happens):
    // emptied, the Style set, the texts stored, then Layout
    __declspec(noinline) void SetNameDesc(const TCHAR* InName, const TCHAR* InDesc, INT InStyle);
    // 0x1002ea70 (name ours): bShowCount is 1, 2 or 3: the amount follows the name
    __declspec(noinline) UBOOL ShowsCount();
    // 0x10030770 (ret 0xC): the tip of an item entry (or the skill / action / shortcut / recipe the entry
    // stands for) for a Style and Options
    __declspec(noinline) void Fill(NItemEntry* Item, DWORD InStyle, DWORD InOptions);
    // 0x10031a70 (ret 8): a macro object's tip: Macro is an NMacroEntry; its name and description (NMacroEntry::GetName /
    // GetDescription, 0x100A7510 / 0x100A7550) fill this tip
    __declspec(noinline) void FillMacro(NToolTipInfo* Macro, DWORD InStyle);
    // 0x10031b10 (ret 8): the party matching row's tip: the level, the class and the party's purpose, and the
    // row's text as the description (`this` is the row's NToolTipInfo)
    __declspec(noinline) void FillMatchRow(NListRow* Row, DWORD InStyle);

    // 0x1002e910 (cdecl; name ours): Value with a comma between thousands, in a static buffer
    static const TCHAR* FormatNumber(INT Value);
    // 0x1002e9c0 (cdecl; name ours): the color of a formatted amount: light grey up to four digits, then pink, yellow,
    // green and cyan in turn as it grows
    static DWORD NumberColor(const TCHAR* Text);
};
NW_ASSERT_OFFSET(NToolTipInfo, Enchant, 0x1C);
NW_ASSERT_OFFSET(NToolTipInfo, Type, 0x30);
NW_ASSERT_OFFSET(NToolTipInfo, ItemID, 0x38);
NW_ASSERT_OFFSET(NToolTipInfo, Lines, 0x40);
NW_ASSERT_OFFSET(NToolTipInfo, Description, 0x4C);
NW_ASSERT_OFFSET(NToolTipInfo, Style, 0x58);
NW_ASSERT_OFFSET(NToolTipInfo, FullBox, 0x60);
NW_ASSERT_OFFSET(NToolTipInfo, TitleBox, 0x70);
NW_ASSERT_SIZE(NToolTipInfo, 0x80);

struct NToolTipSkin
{
    UTexture* Tex[9];       // +0x00 "L2UI.Tooltip.Tool01" .. "Tool09" (corners, edges and fill, row by row), set by NCWnd::OnCreate
    NToolTipInfo Info;      // +0x24 the copy of the tip being drawn (Draw / DrawFull store it first)

    __declspec(noinline) NToolTipSkin();     // 0x1002dba0
    __declspec(noinline) ~NToolTipSkin();    // 0x1002dbc0: the arrays and strings are emptied by hand, then the
                                             //   members go (0x100025a0)

    // 0x1002df40 (ret 0x98): the box of the title line (TitleBox + 10) with the title text (DrawTitleText) at the
    // position X, Y the owner gives (relative to the device's origin, the window's corner), which the routine moves to
    // keep the box on the screen (C3: by value; C2 took pointers). Info is taken BY VALUE: the routine destroys its copy.
    // Nothing is drawn for a tip with no name and no lines.
    void Draw(NCGDevice* Device, NToolTipInfo Info, INT X, INT Y, NToolTipRect Wnd);
    // 0x1002dc40 (ret 0x98): the same for the full box (FullBox + 10) with DrawFullText
    void DrawFull(NCGDevice* Device, NToolTipInfo Info, INT X, INT Y, NToolTipRect Wnd);
    // 0x1002c0f0 (ret 0xC, name ours): the title line at X, Y: "+N" (orange), the name, the additional name
    // (yellow), the grade, " (count)", the 0x1000 style's list and the first property line when Options has
    // NTTO_Price / NTTO_PriceEach
    void DrawTitleText(NCGDevice* Device, INT X, INT Y);
    // 0x1002b800 (ret 0xC, name ours): the full box's text: the title by Style / ShortCutType, the lines, the description
    void DrawFullText(NCGDevice* Device, INT X, INT Y);
};
NW_ASSERT_SIZE(NToolTipSkin, 0xA4);   // C3: the NToolTipInfo inside grew by 4

class NCCmdTarget : public NCObject
{
    NW_DECLARE_ABSTRACT_CLASS(NCCmdTarget, NCObject, 0, 0x102B8220)
    // ctor: NCObject's (0x10020ca0, identical code folded); ~NCCmdTarget 0x10001690
};
NW_ASSERT_SIZE(NCCmdTarget, 0x34);

// A position and a FLOAT size in screen pixels (name ours): what NCWnd hands PtInRect by value, and what
// GetPositioningRect / ClipToParent fill.
struct NRectF
{
    INT X;
    INT Y;
    FLOAT W;
    FLOAT H;
};
NW_ASSERT_SIZE(NRectF, 0x10);

class NCWnd : public NCCmdTarget
{
    NW_DECLARE_CLASS(NCWnd, NCCmdTarget, 0, 0x102C6BC8)

    // The sides of the sticky windows of Src\NWnd.cpp (names ours; the assert strings say SD_MAX). The order is the
    // one of GetOppStickDirection's pairs: 0 <-> 3, 1 <-> 2.
    enum { SD_TOP = 0, SD_LEFT = 1, SD_RIGHT = 2, SD_BOTTOM = 3, SD_MAX = 4 };
    // Bits of PosFlags (names ours): RearrangePosition sets POS_REARRANGED while it moves a window along, SetWindowSize
    // clears POS_REARRANGED | POS_SIZED on the children before it rearranges them.
    enum { POS_REARRANGED = 1, POS_SIZED = 2 };

    // C3 layout, 0xC8 bytes (C2: 0x98). The local rectangle (LocalX/Y/W/H) is gone, the size is FLOAT, the icon
    // moved up, the sticking was reworked (four arrays of entries) and there is an anchor.
    NWndListNode* ChildHead;   // +0x34
    NWndListNode* ChildTail;   // +0x38
    NCIcon* Icon;              // +0x3C shown instead of the window while minimized (NWS_MINIMIZABLE)
    UBOOL bFocus;              // +0x40
    union
    {
        struct
        {
            INT ScreenX;       // +0x44 absolute
            INT ScreenY;       // +0x48
            FLOAT Width;       // +0x4C
            FLOAT Height;      // +0x50
        };
        NRectF Rect;           // the same four: IsInWindow hands them to PtInRect by value
    };
    // C4: SetPosition has three more arguments (a window can be pinned to the screen's edges or centre: its offset from
    // that edge is kept here so a change of the screen size can put the window back, see ResizeWindows)
    INT PinTypeX;              // +0x54 (name ours) SetPosition's 6th argument: 0 the left edge, 1 the right edge, 2 by the width, 5 the centre
    INT PinTypeY;              // +0x58 (name ours) the 7th: 2 by the height, 3 the top (the default), 4 the bottom, 5 the centre
    INT PinOffsetX;            // +0x5C (name ours) X, GSEKScreenX - X, W or GSEKScreenX / 2 - X by PinTypeX
    INT PinOffsetY;            // +0x60 (name ours) Y, GSEKScreenY - Y, H or GSEKScreenY / 2 - Y by PinTypeY
    BYTE bPinned;              // +0x64 (name ours) SetPosition's 5th argument was set
    BYTE _Unknown65[3];        // +0x65
    DWORD Style;               // +0x68 NWS_*
    UBOOL bMouseDown;          // +0x6C (name ours) OnLButtonDown sets it, OnLButtonUp clears it; OnMouseMove drags
                               //   while it is set
    DWORD PosFlags;            // +0x70 (name ours) POS_*
    NWndEntry* Entry;          // +0x74 own registration
    UBOOL bEnabled;            // +0x78
    NCFrameCtrl* FrameCtrl;    // +0x7C title bar (NWS_TITLEBAR)
    INT FrameID;               // +0x80 id given to FrameCtrl (GNextFrameID++)
    INT DragMouseX;            // +0x84 last mouse position of a drag
    INT DragMouseY;            // +0x88
    NToolTipSkin* ToolTipSkin; // +0x8C made by OnCreate for every window, freed by OnDestroy (name ours)
    NWndEntry* NextTab;        // +0x90 TAB goes there
    NWndEntry* PrevTab;        // +0x94 Shift+TAB goes there
    INT Unknown78;             // +0x98 no function of NCWnd touches it (C2: the window number of the windows INI)   // C2: offset derived, no evidence
    INT ClipOffsetX;           // +0x9C (name ours) ClipToParent: how far the left edge was moved into the parent
    INT ClipOffsetY;           // +0xA0 the same for the top edge
    TArray<NWndEntry*> StuckWnds[SD_MAX];   // +0xA4 the windows stuck to each side (InitStuckState empties them)
    INT AnchorType;            // +0xD4 SetAnchor: 0 none, 1..9 where in the parent the window is held (names ours)
    INT AnchorOffsetX;         // +0xD8
    INT AnchorOffsetY;         // +0xDC
    FLOAT ScaleX;              // +0xE0 RearrangePosition: the share of a size change of the group the window follows   // C2: offset derived, no evidence
    FLOAT ScaleY;              // +0xE4   // C2: offset derived, no evidence

    NCWnd();                                                     // 0x10036ce0

    virtual __declspec(noinline) INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam); // 25 0x10034390
    virtual UBOOL IsEnabled() { return bEnabled; }               // 26 0x10003020
    virtual UBOOL IsInWindow(INT X, INT Y);                      // 27 0x100349f0 (name ours)
    virtual UBOOL IsFocused() { return bFocus; }                 // 28 0x10003030 (name ours)
    virtual void SetFocus();                                     // 29 0x10035b10
    virtual void KillFocus();                                    // 30 0x100347a0
    virtual void SetStyle(DWORD NewStyle) { Style = NewStyle; }  // 31 0x100347c0
    virtual DWORD GetStyle() { return Style; }                   // 32 0x10003040
    virtual INT vfn032() { return 0; }                           // 33 0x100023a0 (folded) 1 = takes keyboard input
    virtual INT vfn033() { return 0; }                           // 34 0x100023a0 (folded)
    virtual void NewVirtual35();  // 35 0x100023a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnTimer(INT TimerID) { return 0; }               // 36 0x100023d0 (folded) WM_TIMER
    virtual INT OnCreate();                                      // 37 0x10035f20
    virtual INT OnDestroy();                                     // 38 0x10036160
    virtual void NewVirtual39();  // 39 0x100351d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // C4: a virtual now (C3: a plain method, 4 arguments); the 5th to 7th are the screen pin: the stock callers pass 0, 0, 3
    // as a rule (NCWnd::OnCreate: SetPosition(0, -15, W, 15, 0, 0, 3))
    virtual void SetPosition(INT X, INT Y, DWORD W, DWORD H, UBOOL bPin = 0, INT PinX = 0, INT PinY = 3);   // 40 0x10035c60 X, Y
                                                                 //   relative to the parent; W and H 0 keep the size
    virtual UBOOL IsShowWindow();                                // 41 0x100345e0 (name ours)
    virtual void NewVirtual42();  // 42 0x100348b0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual UBOOL PtInRect(INT X, INT Y, NRectF Rect);           // 43 0x10034860 (edges included)
    virtual void NewVirtual44();  // 44 0x10034950 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void SetTabOrder(NWndEntry* Next, NWndEntry* Prev);  // 45 0x10034550 (name ours)
    virtual void EnableWindow(UBOOL bEnable);                    // 46 0x10034570 (name ours)
    virtual void SetWindowSize(FLOAT W, FLOAT H);                // 47 0x10036dc0
    virtual void ShowWindow();                                   // 48 0x100345a0
    virtual void HideWindow();                                   // 49 0x10035ab0
    virtual void NewVirtual50();  // 50 0x100347d0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void MoveWindow(INT DX, INT DY);                     // 51 0x10037a20 (name ours) snaps
    virtual void OffsetWindow(INT DX, INT DY);                   // 52 0x10035410 (name ours)
    virtual void Restore();                                      // 53 0x100356e0 (name ours)
    virtual void Minimize();                                     // 54 0x10034a50 (the db says MinimizeWindow)
    virtual void CenterInParent();                               // 55 0x10035ea0 (name ours)
    virtual void SetFrameTitleID(INT ID);                        // 56 0x10034bc0 (C4 only, name ours; ret 4) the title bar's (FrameCtrl, +0x7C) SysString title;
                                                                 //   overridden by the windows whose bar is a field of their own (L2ChatWnd, NCPetWnd, NCRecipeShopWnd,
                                                                 //   NCTradeWnd). Not called SetTitleID: NCFrameCtrl::SetTitleID (0x10011810, not a virtual) would override it
    virtual void SetTitle(const TCHAR* Title);                   // 57 0x10034b90 (name ours)
    virtual void SetCapture();                                   // 58 0x10034c50 (guard; C3 SetItemSelect: NWS_ITEMSELECT)
    virtual void ReleaseCapture();                               // 59 0x10034cd0 (guard)
    virtual void SetModal(NWndEntry* E);                         // 60 0x10034c10 (name ours)
    virtual void ClearModal(NWndEntry* E);                       // 61 0x10034c30 (name ours)
    virtual INT OnPaint(NCGDevice* Device) { return 0; }         // 62 0x100023d0 (folded) WM_PAINT
    virtual INT OnDrawToolTip(NCGDevice* Device) { return 0; }   // 63 0x100023d0 (folded) (name ours)
    virtual INT OnChar(WPARAM wParam, LPARAM lParam) { return 0; }          // 64 0x100023b0 (folded)
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);                    // 65 0x10034ad0
    virtual INT OnKeyUp(WPARAM wParam, LPARAM lParam);                      // 66 0x10034b60
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);                  // 67 0x10036240
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);                // 68 0x10036350
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                  // 69 0x10036410 (guard: OnLButtonDown)
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam) { return 0; }   // 70 0x100023b0 (folded)
    virtual INT OnRButtonUp(WPARAM wParam, LPARAM lParam) { return 0; }     // 71 0x100023b0 (folded)
    virtual INT OnMove(WPARAM wParam, LPARAM lParam);                       // 72 0x10034aa0 WM_MOVE
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam) { return 0; }    // 73 0x100023b0 (folded)

    // non-virtual
    // This is NCCmdTarget::WindowProc (vslot 25 of NCCmdTarget's vtable): WM_CREATE, WM_DESTROY and WM_TIMER go to
    // OnCreate / OnDestroy / OnTimer. NCWnd::WindowProc calls it first.
    __declspec(noinline) INT DefWindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);   // 0x10005a90 (name ours)
    void SetHeadWindow(NWndEntry* Child);                        // 0x100345f0 (guard; C3 BringChildToTop) Child to the head of the list
    void BringChildToTop(NWndEntry* Child) { SetHeadWindow(Child); }   // the C3 name
    NWndEntry* FindWindowAt(INT X, INT Y);                       // 0x10034a40 (name ours)
    // The C4 virtuals the header keeps as NewVirtual<N> placeholders (NNewVirtuals.gen.cpp defines those): the real members,
    // hooked by the stock address, and the calls through the slot
    UBOOL IsInclude(INT X, INT Y);                               // 0x100348b0 (slot 42, guard) the point is in the rectangle
    UBOOL IsTopWindowAt(INT X, INT Y);                           // 0x10034950 (slot 44, guard) NCVirtualWndMain's slot 36 says so
    void MoveWindowAbs(INT X, INT Y);                            // 0x100347d0 (slot 50, guard) MoveWindow to the screen position
    UBOOL CallIsTopWindowAt(INT X, INT Y) { return ::nw::VirtualCall<UBOOL>(this, 44, X, Y); }
    void FormatNumber(TCHAR* Out, const TCHAR* Digits);          // 0x10034d50 (name ours)
    UBOOL AddChildNode(NWndEntry* Child);                        // 0x100350d0 (name ours)
    void RemoveChildNode(NWndEntry* Child);                      // 0x10035160 (name ours)
    NWndEntry* AddChildWindow(NCWnd* Child, DWORD ChildStyle);   // 0x10035840 (name ours)
    void RemoveChildWindow(NWndEntry* Child);                    // 0x10035880 (name ours)
    void SetScreenPos(INT X, INT Y, const POINT* Origin);        // 0x10035e50 (name ours)
    void Snap(POINT* Delta);                                     // 0x10037150 (name ours) sticky windows

    // The second half of CreateChild on the child (name ours): an out-of-line copy of the
    // inline code for the call sites where the stock compiler stopped inlining it.
    void AttachEntry(NWndEntry* E);                              // 0x10006470

    // Registers a newly created child the way every OnCreate does (inlined in the stock
    // code, name ours): entry under this window, screen position made absolute, WM_CREATE.
    // Forced inline: inside a guard (/EHa) our compiler kept it out of line.
    FORCEINLINE NWndEntry* CreateChild(NCWnd* Child, DWORD ChildStyle)
    {
        NWndEntry* E = AddChildWindow(Child, ChildStyle);
        Child->Entry = E;
        if (E->Parent)
        {
            NCWnd* Parent = E->Parent->Wnd;
            Child->ScreenX += Parent->ScreenX;
            Child->ScreenY += Parent->ScreenY;
        }
        Child->WindowProc(WM_CREATE, 0, 0);
        return E;
    }

    // Sticky windows and anchors (Src\NWnd.cpp, new in C3; the names of the ones with a guard() are the stock ones)
    void InitStuckState();                                       // 0x10035bc0 empties the four lists
    __declspec(noinline) UBOOL IsAnchored();                     // 0x10034f40 (name ours)
    __declspec(noinline) UBOOL IsDragable();                     // 0x10034f50 (name ours)
    __declspec(noinline) void AddPosFlag(DWORD Flag);            // 0x10034bd0 (name ours)
    __declspec(noinline) void ClearPosFlag(DWORD Flag);          // 0x10034be0 (name ours)
    __declspec(noinline) DWORD GetPosFlags();                    // 0x10034c00 (name ours)
    NCWnd* GetDragableParentWnd();                               // 0x10035960 the nearest ancestor with NWS_DRAGABLE
    __declspec(noinline) NRectF* GetLocalRect(NRectF* Out);          // 0x10035650 position relative to the parent (name ours)
    __declspec(noinline) INT* GetLocalPos(INT* Out);             // 0x100356a0 the same, only the position (name ours)
    INT GetOppStickDirection(INT Direction);                     // 0x10034e80
    void SetAnchor(INT Type, INT OffsetX, INT OffsetY);          // 0x100358c0
    void MoveChildWindow(INT DX, INT DY);                        // 0x100351d0 moves the window and its subtree
    void ClipToParent(NRectF* Out);                              // 0x10035420 (name ours)
    void GetStuckWndList(TArray<NCWnd*>& a_StuckWndList);        // 0x100364d0 this and everything stuck to it
    UBOOL IsStuck(INT Direction, NWndEntry* Wnd);                // 0x100365c0 -1 any direction, -1 / -2 as Wnd
    void StickWnd(INT Direction, NWndEntry* Wnd);                // 0x10037010
    void UnstickWnd(INT Direction, NWndEntry* Wnd);              // 0x10036bf0
    void GetPositioningRect(NRectF* a_pRect, NCWnd** ppLeft, NCWnd** ppRight, NCWnd** ppTop, NCWnd** ppBottom);   // 0x10036870
    void RearrangePosition(FLOAT OldW, FLOAT OldH, FLOAT NewW, FLOAT NewH);   // 0x10036a10
    UBOOL FocusFirstInput();                                     // 0x10034740 (name ours)
    UBOOL HasSliderBarChild();                                   // 0x10035020 (name ours)
    UBOOL FocusSliderBarParent();                                // 0x10035060 (name ours)
    void FocusNextWindow(NWndEntry* Hidden);                     // 0x10034f60 (name ours)
};
NW_ASSERT_OFFSET(NCWnd, ChildHead, 0x34);
NW_ASSERT_OFFSET(NCWnd, Icon, 0x3C);
NW_ASSERT_OFFSET(NCWnd, bFocus, 0x40);
NW_ASSERT_OFFSET(NCWnd, ScreenX, 0x44);
NW_ASSERT_OFFSET(NCWnd, Width, 0x4C);
NW_ASSERT_OFFSET(NCWnd, PinTypeX, 0x54);
NW_ASSERT_OFFSET(NCWnd, PinOffsetY, 0x60);
NW_ASSERT_OFFSET(NCWnd, bPinned, 0x64);
NW_ASSERT_OFFSET(NCWnd, Style, 0x68);
NW_ASSERT_OFFSET(NCWnd, bMouseDown, 0x6C);
NW_ASSERT_OFFSET(NCWnd, PosFlags, 0x70);
NW_ASSERT_OFFSET(NCWnd, Entry, 0x74);
NW_ASSERT_OFFSET(NCWnd, bEnabled, 0x78);
NW_ASSERT_OFFSET(NCWnd, FrameCtrl, 0x7C);
NW_ASSERT_OFFSET(NCWnd, ToolTipSkin, 0x8C);
NW_ASSERT_OFFSET(NCWnd, NextTab, 0x90);
NW_ASSERT_OFFSET(NCWnd, ClipOffsetX, 0x9C);
NW_ASSERT_OFFSET(NCWnd, StuckWnds, 0xA4);
NW_ASSERT_OFFSET(NCWnd, AnchorType, 0xD4);
NW_ASSERT_OFFSET(NCWnd, ScaleX, 0xE0);
NW_ASSERT_SIZE(NCWnd, 0xE8);

#pragma pack(pop)