#pragma once
// NCScrollWnd - window whose content scrolls vertically (UClass "CScrollWnd", 0xCC): base of
// the HTML viewers, item windows and quest/notice info windows. ScrollY is the content offset
// (0 at the top, down to MinScrollY = Height - content height); Delta* is how much the last
// change moved it, for the subclasses that repaint incrementally. Scroll up / down buttons and
// a slider sit on the right edge; the keyboard (PageUp/PageDown/arrows) and the wheel scroll.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPushButton;
class NCSliderBar;

#if NW_HOOK_MODE
#define GNextScrollCtrlID NW_GLOBAL(INT, 0x10226570)   // ids of the scroll controls, from 0x1200 (C4 address; read from OnCreate)
#else
extern INT GNextScrollCtrlID;
#endif

class NCScrollWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCScrollWnd, NCWnd, 0, 0x102C3F50)

    INT ScrollX;                     // +0xE8 never changed here
    INT ScrollY;                     // +0xEC <= 0
    INT DeltaX;                      // +0xF0 previous - current, after each scroll
    INT DeltaY;                      // +0xF4
    INT PageStep;                    // +0xF8 (100) PageUp / PageDown
    INT LineStep;                    // +0xFC (15) arrows, buttons, wheel (3 per notch)
    INT MinScrollY;                  // +0x100 Height - content height (SetContentHeight)
    NCPushButton* ScrollUpButton;    // +0x104
    NCPushButton* ScrollDownButton;  // +0x108
    INT ScrollUpID;                  // +0x10C
    INT ScrollDownID;                // +0x110
    NCSliderBar* ScrollBar;          // +0x114
    INT ScrollBarID;                 // +0x118
    INT Unknown0C8;                  // +0x11C (12; named after its C1 offset)   // C2: offset derived, no evidence

    NCScrollWnd();                                                   // 0x1002c040
    // ~NCScrollWnd                                                  // 0x100124c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1002b5a0
    virtual INT OnCreate();                                          // 37 0x1002bd60
    virtual INT OnDestroy();                                         // 38 0x1002b880
    virtual INT OnPaint(NCGDevice* Device) { return 1; }             // 62 0x100f76f0 (folded)
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x1002b980
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x1002bc60
    // C3: a virtual of this class (C2 overrode NCWnd's SetSize): the size as DWORDs, then the controls follow the
    // right edge (name ours)
    virtual void SetSize(DWORD W, DWORD H);                          // 74 0x1002b640
    virtual INT ScrollUp();                                          // 75 0x1002bac0
    virtual INT ScrollDown();                                        // 76 0x1002bb50
    virtual INT OnScrollBar(INT Value);                              // 77 0x1002bbf0 (the slider's value, a line step multiple)
    // not virtual: kept so that NNewVirtuals.gen.cpp (generated: the placeholders of slots 68 and 71) still compiles
    // C4: void NewVirtual68();
    // C4: void NewVirtual71();

    void ShowBars(INT bShow);                                        // 0x1002b6e0 (name ours)
    void SetContentHeight(INT ContentHeight);                        // 0x1002b750
    // C3 (name ours): the other setter of the content height (the tree, lists and item windows call it): a bar whose
    // thumb already passes Height - 30 is reset to 0, else the range is the whole content height
    void SetContentHeightEx(INT ContentHeight);                      // 0x1002b7b0
    void SetRepeatDelay(INT Delay);                                  // 0x1002b850
    virtual void ShowWindow();  // 48 0x100efc40 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCScrollWnd, ScrollY, 0xEC);
NW_ASSERT_OFFSET(NCScrollWnd, MinScrollY, 0x100);
NW_ASSERT_OFFSET(NCScrollWnd, ScrollBar, 0x114);
NW_ASSERT_SIZE(NCScrollWnd, 0x120);

#pragma pack(pop)