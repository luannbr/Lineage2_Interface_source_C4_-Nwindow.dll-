#pragma once
// NCFrameCtrl - title bar of a window (UClass "CFrameCtrl", 0x238) and NCResizeFrame, the
// edge that resizes a window (UClass "CResizeFrame", 0x254).
//
// Dragging the bar sends WM_MOVE(delta) to the parent, which moves (and snaps). A decorated
// frame paints its textures (the 0x14 high bar, or the small horizontal / vertical bars) and
// the title, and adds a minimize button (parent NWS_MINIMIZABLE) and a close button (unless
// NWS_NOCLOSE); NCWnd::OnCreate makes undecorated ones: an invisible 15 px drag area above
// windows with NWS_TITLEBAR.
//
// C3 layout (0x238), from the constructors 0x100102f0 / 0x10010450 and OnCreate / OnPaint /
// OnMouseMove / OnLButtonDown / SetTitleText: C2 had six dwords less in the texture list and
// kept the title position and the drag state at other offsets.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NWM_COMMAND ids of the frame buttons
enum { NID_FRAME_MINIMIZE = 0xF0F0, NID_FRAME_CLOSE = 0xF0F1, NID_FRAME_EXPAND = 0xF0F2 };

// C4 (names ours): the state of the expand button of a frame bar, 16 bytes at +0x24C
struct NFrameExpand
{
    NCButton* Button;          // NULL until CreateExpandBtn
    INT CollapsedH;            // CreateExpandBtn's first argument: the parent's height while the bar is not expanded
    INT ExpandedH;             // the second: the height while it is expanded (the state after the first click)
    INT bExpanded;
};

class NCFrameCtrl : public NControl
{
    NW_DECLARE_CLASS(NCFrameCtrl, NControl, 0, 0x102BB108)

    FString Title;             // +0x1F4 the window's title (FString: Num at +0x1D8 tells whether there is one)
    INT TitleID;               // +0x200 (name ours) C4: the title as a SysString index (-1 = Title); SetTitleID sets it and empties Title
    BYTE _Unknown204[4];       // +0x204 (nothing writes or reads it)
    UTexture* BackLeftTex;     // +0x208 "L2UI_CH3.FrameCtrl.FrameBackLeft", 16x20 (C4: the constructors store it here)
    UTexture* BackMidTex;      // +0x20C "L2UI_CH3.FrameCtrl.FrameBackMid", stretched (the source is 32x20)
    UTexture* BackRightTex;    // +0x210 "L2UI_CH3.FrameCtrl.FrameBackRight", 16x20
    NCButton* CloseButton;     // +0x214
    NCButton* MinimizeButton;  // +0x218
    UTexture* SmallBarTex[6];  // +0x21C "L2UI_CH3.FrameCtrl.smallbar_h1" .. "_h3" (the 8x12 horizontal bar: [0] and
                               //   [2] are its two caps, [1] the middle) then "smallbar1" .. "smallbar3" (the 12x8
                               //   vertical one: [3] top, [4] middle, [5] bottom)
    INT LastMouseX;            // +0x234 mouse position of the drag
    INT LastMouseY;            // +0x238
    INT TitleX;                // +0x23C the title's position; 0, 0 until OnPaint measures it (SetTitleText clears it)
    INT TitleY;                // +0x240
    INT bVertical;             // +0x244 (name ours) constructor argument 2: the small bar is the vertical one
    INT bSmall;                // +0x248 (name ours) constructor argument 3: the small bar instead of the title bar
    // C4: the expand button of the target window's bar (CreateExpandBtn, OnExpandBtnClick): a small button that shrinks the
    // window to a height and back (the parent's SetWindowSize) and tells the owner (NWM_COMMAND 0xFFFF expanded, 0xFFFE not)
    NFrameExpand Expand;       // +0x24C (name ours) NULL button until CreateExpandBtn
    INT GrabX;                 // +0x25C where the drag grabbed the bar (NCWnd::Snap)
    INT GrabY;                 // +0x260
    INT bDecorated;            // +0x264 constructor argument 1 (1 for the default constructor): textures and buttons
    INT bMoved;                // +0x268 (name ours) the last mouse move of the drag moved the window

    // out of line: NCResizeFrame's constructor and NCWnd::OnCreate call them
    __declspec(noinline) NCFrameCtrl();                              // 0x10012560 (bDecorated 1)
    // 0x100126e0: A is bDecorated, B bVertical, C bSmall (the C2 client had the one-argument constructor only)
    __declspec(noinline) NCFrameCtrl(INT InDecorated, INT InVertical = 0, INT InSmall = 0);
    // ~NCFrameCtrl                                                  // 0x100124c0

    virtual __declspec(noinline) INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100115b0
    virtual INT OnCreate();                                          // 37 0x100121c0
    virtual INT OnDestroy();                                         // 38 0x100119b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10011c00
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10011fe0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10011750
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100117b0
    virtual void SetFrameSize(DWORD W, DWORD H);                     // 75 0x10011670 (name ours)
    virtual INT OnClose();                                           // 76 0x10011920 (name ours)
    virtual INT OnMinimize();                                        // 77 0x10011650 (name ours)
    virtual void NewVirtual78();  // 78 0x10011ab0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // 0x10011ab0 (guard OnExpandBtnClick): the body of slot 78, hooked by its address under this name (the slot's own definition
    // is the generated stub); the bar's WindowProc calls the slot for the button's id 0xF0F2
    INT OnExpandBtnClick();
    // 0x10012380 (ret 8, guard CreateExpandBtn): makes the expand button at (1, 6), 11 x 13
    void CreateExpandBtn(INT InCollapsedH, INT InExpandedH);

    void SetTitleText(const TCHAR* InTitle);                         // 0x100117e0 (name ours)
    void SetTitleID(INT ID);                                         // 0x10011810 (anonymous in the db, name ours; NCWnd slot 56 calls it)
    // 0x1000f820 (ret 8; anonymous in the db, name ours): the bar's shape, constructor arguments 2 and 3 after the fact (the
    // shortcut window turns its bar vertical and small as the window is laid out)
    void SetBarShape(INT InVertical, INT InSmall);
};
NW_ASSERT_OFFSET(NCFrameCtrl, Title, 0x1F4);
NW_ASSERT_OFFSET(NCFrameCtrl, TitleID, 0x200);
NW_ASSERT_OFFSET(NCFrameCtrl, BackLeftTex, 0x208);
NW_ASSERT_OFFSET(NCFrameCtrl, BackRightTex, 0x210);
NW_ASSERT_OFFSET(NCFrameCtrl, Expand, 0x24C);
NW_ASSERT_SIZE(NFrameExpand, 0x10);
NW_ASSERT_OFFSET(NCFrameCtrl, CloseButton, 0x214);
NW_ASSERT_OFFSET(NCFrameCtrl, MinimizeButton, 0x218);
NW_ASSERT_OFFSET(NCFrameCtrl, SmallBarTex, 0x21C);
NW_ASSERT_OFFSET(NCFrameCtrl, LastMouseX, 0x234);
NW_ASSERT_OFFSET(NCFrameCtrl, TitleX, 0x23C);
NW_ASSERT_OFFSET(NCFrameCtrl, bVertical, 0x244);
NW_ASSERT_OFFSET(NCFrameCtrl, GrabX, 0x25C);
NW_ASSERT_OFFSET(NCFrameCtrl, bDecorated, 0x264);
NW_ASSERT_OFFSET(NCFrameCtrl, bMoved, 0x268);
NW_ASSERT_SIZE(NCFrameCtrl, 0x26C);

// resize direction: which mouse delta goes to the parent (NWM_COMMAND, lParam = delta)
enum { RESIZE_Horizontal = 1, RESIZE_Vertical = 2 };

class NCResizeFrame : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCResizeFrame, NCFrameCtrl, 0, 0x102BB638)

    INT ResizeGrabX;           // +0x26C where the edge was grabbed, relative to the window (OnLButtonDown)
    INT ResizeGrabY;           // +0x270
    UBOOL bPressed;            // +0x274 cleared with bDragging, never read here
    INT Direction;             // +0x278 RESIZE_*
    INT Unknown248;            // +0x27C (the constructor clears it; nothing reads it)
    UBOOL bDragging;           // +0x280 button went down inside
    UBOOL bCursorLocked;       // +0x284 resize cursor shown and locked

    NCResizeFrame(INT InDirection = 0);                              // 0x10012870
    // ~NCResizeFrame                                                // 0x100128f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0
    virtual void KillFocus();                                        // 30 0x10011900
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10012080
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10011830
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100118d0
};
NW_ASSERT_OFFSET(NCResizeFrame, ResizeGrabX, 0x26C);
NW_ASSERT_OFFSET(NCResizeFrame, Direction, 0x278);
NW_ASSERT_OFFSET(NCResizeFrame, bCursorLocked, 0x284);
NW_ASSERT_SIZE(NCResizeFrame, 0x288);

#pragma pack(pop)
