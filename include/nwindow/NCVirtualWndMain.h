#pragma once
// NCAbstractMain / NCVirtualWndMain - owner of every window: registration, z-order lists,
// hit tests, focus, message dispatch, cursors (UClass "CAbstractMain", "CVirtualWndMain").
//
// Input path: the console calls DispatchWndMsg with the Win32 message; mouse coordinates are
// made screen-relative (ClientToScreenPacked), then the top-level windows are tried from the
// front (TopLevel is painted from index 0, so the last one is on top): clicks go to the deepest
// window hit (SetFocusingWindow, which also gives it the focus), mouse moves to the one under the
// cursor or to the focused one while the left button is held (PassToMouseMoveWindow), keyboard and IME
// messages to the focused window (PassToFocusedWindow). WM_PAINT paints each tree back to front and
// then lets the window under the cursor draw its tool tip.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class UTexture;

class NCAbstractMain : public NCObject
{
    NW_DECLARE_ABSTRACT_CLASS(NCAbstractMain, NCObject, 0, 0x102C7638)
    // constructor: NCObject's (0x10020ca0, identical code folded); ~NCAbstractMain 0x10024690

    virtual NWndEntry* RegisterNWindow(NCWnd* Wnd, DWORD Style, NWndEntry* Parent) = 0;  // 25 0x10171e13
    virtual void UnRegisterNWindow(NWndEntry* Entry) = 0;                                 // 26 0x10171e13
    virtual NWndEntry* FindNWindow(INT X, INT Y) = 0;                                     // 27 0x10171e13
    virtual void ClearFocus() = 0;                                                        // 28 0x10171e13
    virtual void AddTopLevel(NWndEntry* Entry) = 0;                                       // 29 0x10171e13 (name ours)
    // new in C2 (names ours; bodies 0x1002ef60 / 0x1002ef90 / 0x1002d410): a stack of windows that took the
    // focus for themselves (the array at +0x48 of NCVirtualWndMain): the last one gets the focus back when
    // another leaves
    virtual void PushFocusWindow(NWndEntry* Entry) = 0;                                   // 30 0x10171e13
    virtual void PopFocusWindow(NWndEntry* Entry) = 0;                                    // 31 0x10171e13
#if NW_HOOK_MODE
    virtual void NewVirtual32();  // 32 0x10171e13 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual33();  // 33 0x10171e13 (only this client has it; defined in NNewVirtuals.gen.cpp)
#else
    virtual void NewVirtual32() = 0;  // 32 0x10171e13 (_purecall in the stock; the hook build keeps a stub that jumps to it)
    virtual void NewVirtual33() = 0;  // 33 0x10171e13
#endif
    virtual INT NumFocusWindows() = 0;                                                    // 34 0x10171e13
#if NW_HOOK_MODE
    virtual void NewVirtual35();  // 35 0x10171e13 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual36();  // 36 0x10171e13 (only this client has it; defined in NNewVirtuals.gen.cpp)
#else
    virtual void NewVirtual35() = 0;  // 35 0x10171e13
    virtual void NewVirtual36() = 0;  // 36 0x10171e13
#endif
};
NW_ASSERT_SIZE(NCAbstractMain, 0x34);

// the array of window records with the out-of-line copy of AddUniqueItem the stock code calls (name ours)
struct NWndEntryArray : public TArray<NWndEntry*>
{
    __declspec(noinline) INT AddUniqueItem(NWndEntry* const& Item);      // 0x10039b70
};

class NCVirtualWndMain : public NCAbstractMain
{
    NW_DECLARE_CLASS(NCVirtualWndMain, NCAbstractMain, 0, 0x102C7B68)

    NCGDevice* GDevice;              // +0x34 created by the constructor
    DWORD NumWindows;                // +0x38 registered windows (RegisterNWindow refuses 1000+)
    TArray<NWndEntry*> Windows;      // +0x3C every registered window
    TArray<NWndEntry*> TopLevel;     // +0x48 windows without a parent, back to front
    TArray<NWndEntry*> FocusWindows; // +0x54 new in C2 (name ours): the stack PushFocusWindow / PopFocusWindow keep
    NWndEntryArray CapturedWindows;  // +0x60 new in C4 (name ours; the stock's guard strings call the entries hCapturedWnd): the windows
                                     //   that capture the mouse, AddCapturedWindow / DeleteCapturedWindow; every WM_MOUSEMOVE goes to them first
    TArray<NWndEntry*> TopMost;      // +0x6C windows registered with NWS_TOPMOST
    HCURSOR Cursors[7];              // +0x78 from this package's resources (LoadCursors)
    HCURSOR ArrowCursor;             // +0x94 IDC_ARROW
    INT CursorType;                  // +0x98 index in Cursors, 4 = color cursor, -1 = arrow
    UBOOL bCursorEnabled;            // +0x9C
    HCURSOR ColorCursor;             // +0xA0 built from a texture (CreateColorCursor)
    UBOOL bColorCursor;              // +0xA4 ColorCursor is ours to destroy
    UBOOL bToolTipShown;             // +0xA8 a window already drew its tool tip this frame (the constructor clears it; PostDrawChildWindow sets it)
    UBOOL bMouseOnWindow;            // +0xAC (name ours) PassToMouseMoveWindow: set when a window other than the desktop took the move,
                                     //   cleared for the desktop and by a drag; NConsoleWnd::UpdateCursor shows the normal cursor while it is set
    UBOOL bLButtonDblClk;            // +0xB0 (name ours; C3: a byte global) a double click is under way: WM_LBUTTONDBLCLK sets it, the
                                     //   button up that follows clears it instead of releasing the cursor lock, and a WM_LBUTTONDOWN /
                                     //   WM_RBUTTONDBLCLK that comes while it is set is taken without being routed

    NCVirtualWndMain();                                                    // 0x1003a880
    // ~NCVirtualWndMain                                                   // 0x1003a9d0

    virtual NWndEntry* RegisterNWindow(NCWnd* Wnd, DWORD Style, NWndEntry* Parent); // 25 0x10039be0
    virtual void UnRegisterNWindow(NWndEntry* Entry);                      // 26 0x10039d20
    virtual NWndEntry* FindNWindow(INT X, INT Y);                          // 27 0x10039590
    virtual void ClearFocus();                                             // 28 0x10039460
    virtual void AddTopLevel(NWndEntry* Entry);                            // 29 0x10039bb0 (name ours)
    virtual void PushFocusWindow(NWndEntry* Entry);                        // 30 0x1003a6f0 (name ours)
    virtual void PopFocusWindow(NWndEntry* Entry);                         // 31 0x1003a720 (name ours)
    virtual INT NumFocusWindows();                                         // 34 0x1003dce0 (name ours; the body is shared with other classes' getters)
    virtual NWndEntry* FindChildWindowAt(NCWnd* Wnd, INT X, INT Y);        // 37 0x10039500 (name ours)

    // names ours unless marked; the ones of the guard strings are the stock's
    UBOOL IsDispatchedMsg(UINT Msg);                                       // 0x100380a0
    void SetGDevice(UCanvas* Canvas);                                      // 0x100380d0 (guard name)
    void ReleaseGDevice();                                                 // 0x10038150 (guard name)
    LPARAM ClientToScreenPacked(LPARAM Pos);                               // no C4 function: DispatchWndMsg inlines it
    LPARAM CoordCompensate(LPARAM Pos);                                    // 0x10038550 (guard name)
    UBOOL GetMousePos(INT* X, INT* Y);                                     // 0x10039aa0 (name ours)
    INT PassToFocusedWindow(NWndEntry* Entry, UINT Msg, WPARAM wParam, LPARAM lParam); // 0x100381d0 (guard name; C3: SendToFocused)
    void DrawChildWindow(NWndEntry* Entry);                                // 0x10038e40 (guard name; C3: PaintChildren)
    void LoadCursors();                                                    // 0x10038390
    void DestroyCursors();                                                 // 0x10038420
    void DeleteGDevice();                                                  // 0x10038470
    UBOOL HitTestClick(NWndEntry* Entry, INT X, INT Y);                    // 0x10038620
    INT SetFocusingWindow(NWndEntry* Entry, INT X, INT Y, UINT Msg, WPARAM wParam, LPARAM lParam); // 0x10038790 (guard name; C3: RouteClick)
    UBOOL HitTest(NWndEntry* Entry, LPARAM Pos);                           // 0x10038970
    // C4: bChild is 1 for the recursive calls (the captured windows only get the move at the top)
    INT PassToMouseMoveWindow(NWndEntry* Entry, UINT Msg, WPARAM wParam, LPARAM lParam, INT bChild); // 0x10038a00 (guard name; C3: RouteMouseMove)
    void PostDrawChildWindow(NWndEntry* Entry);                            // 0x10039f40 (guard name; C3: PaintToolTips)
    // the message of a window of the focus stack (guard name; C3: DispatchToFocusWindow)
    INT DispatchWndModalMsg(NWndEntry* Entry, UINT Msg, WPARAM wParam, LPARAM lParam); // 0x10038fa0
    INT DispatchWndMsg(UINT Msg, WPARAM wParam, LPARAM lParam);            // 0x1003a0b0 (guard name)
    void CreateColorCursor(UTexture* Texture);                             // 0x10039680
    void SetCursorType(INT Type, UTexture* Texture);                       // 0x10039a10
    UBOOL IsOverWindow(INT X, INT Y);                                      // 0x10039af0
    // The four virtuals below are NewVirtual32/33/35/36 of NCAbstractMain (placeholders defined in NNewVirtuals.gen.cpp, which
    // jump to the stock code); the real functions are hooked here under the stock's names, non-virtual, so the stock vtable
    // reaches them through the same addresses. When NNewVirtuals.gen.cpp is regenerated these can become the virtuals.
    void AddCapturedWindow(NWndEntry* Entry);                              // 0x1003a780 (slot 32, guard name)
    void DeleteCapturedWindow(NWndEntry* Entry);                           // 0x1003a800 (slot 33, guard name)
    NWndEntry* GetTopWindowAt(INT X, INT Y, NWndEntry* Entry);             // 0x10038c10 (slot 35, guard name)
    UBOOL IsTopWindowAt(INT X, INT Y, NWndEntry* Entry);                   // 0x10038d50 (slot 36, guard name)
    virtual void NewVirtual32();  // 32 0x1003a780 (the stock vtable of this class overrides the placeholder of a parent; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual33();  // 33 0x1003a800 (the stock vtable of this class overrides the placeholder of a parent; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual35();  // 35 0x10038c10 (the stock vtable of this class overrides the placeholder of a parent; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual36();  // 36 0x10038d50 (the stock vtable of this class overrides the placeholder of a parent; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCVirtualWndMain, Windows, 0x3C);
NW_ASSERT_OFFSET(NCVirtualWndMain, TopLevel, 0x48);
NW_ASSERT_OFFSET(NCVirtualWndMain, FocusWindows, 0x54);
NW_ASSERT_OFFSET(NCVirtualWndMain, CapturedWindows, 0x60);
NW_ASSERT_OFFSET(NCVirtualWndMain, TopMost, 0x6C);
NW_ASSERT_OFFSET(NCVirtualWndMain, Cursors, 0x78);
NW_ASSERT_OFFSET(NCVirtualWndMain, ArrowCursor, 0x94);
NW_ASSERT_OFFSET(NCVirtualWndMain, ColorCursor, 0xA0);
NW_ASSERT_OFFSET(NCVirtualWndMain, bToolTipShown, 0xA8);
NW_ASSERT_OFFSET(NCVirtualWndMain, bMouseOnWindow, 0xAC);
NW_ASSERT_OFFSET(NCVirtualWndMain, bLButtonDblClk, 0xB0);
NW_ASSERT_SIZE(NCVirtualWndMain, 0xB4);

#pragma pack(pop)