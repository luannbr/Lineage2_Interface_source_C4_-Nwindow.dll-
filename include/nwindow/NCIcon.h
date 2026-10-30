#pragma once
// NCIcon - the 32x32 icon a minimized window leaves on the screen (UClass "CIcon", 0x2C0).
// NCWnd::OnCreate creates one for NWS_MINIMIZABLE windows; NCWnd::Minimize picks its texture
// and tool tip. It can be dragged (WM_MOVE to itself, sticky like windows); a click without a
// drag restores the window.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCIcon : public NControl
{
    NW_DECLARE_CLASS(NCIcon, NControl, 0, 0x102C1560)

    UTexture* IconTex;         // +0x1F4
    INT DragX;                 // +0x1F8 where the drag grabbed the icon (NCWnd::Snap)
    INT DragY;                 // +0x1FC
    FString Name;              // +0x200 the tool tip text (C1: TCHAR Name[128])
    BYTE bMouseOver;           // +0x20C
    INT Unknown1C0;            // +0x210 (-1; only the constructor writes it; C1 name Unknown2B0)   // C2: offset derived, no evidence
    UBOOL bToolTipAbove;       // +0x214 (SetIcon)
    UTexture* ToolTipTex;      // +0x218 "NWindow.ChatBack"   // C2: offset derived, no evidence
    UBOOL bDragged;            // +0x21C moved since the button went down
    UBOOL bBlink;              // +0x220 (C4; name ours) the icon blinks: set while a flash is going on, cleared by OnLButtonUp (which kills the timer)
    UBOOL bBlinkOn;            // +0x224 (C4; name ours) the highlight is drawn: OnTimer (timer 0xFFF001) flips it, the constructor clears it
    INT BlinkTimerID;          // +0x228 (C4; name ours) the id OnLButtonUp passes to KillTimer (not written by the constructor)

    NCIcon();                                                        // 0x1001e2f0
    // ~NCIcon                                                       // 0x10019180 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1001db30
    virtual void HideWindow();                                       // 49 0x1001ddf0 (C3)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1001de40
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x1001dfb0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x1001dc50
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1001dd10
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x1001dd50
    // slot 68, new in C2 (name ours; WM_LBUTTONDBLCLK goes there): the stock body is `mov eax, 1; ret 8`
    // folded with the other classes' (0x100d2e70)
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam) { return 1; }  // 75 0x100e6a50 (folded)

    void SetIcon(const TCHAR* TexName, INT NameID, BYTE bAbove);     // 0x1001dbe0 (name ours)
    virtual INT OnTimer(INT TimerID) ;  // 36 0x1001de10 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCIcon, Name, 0x200);
NW_ASSERT_OFFSET(NCIcon, bMouseOver, 0x20C);
NW_ASSERT_OFFSET(NCIcon, bToolTipAbove, 0x214);
NW_ASSERT_OFFSET(NCIcon, bDragged, 0x21C);
NW_ASSERT_OFFSET(NCIcon, bBlink, 0x220);
NW_ASSERT_OFFSET(NCIcon, BlinkTimerID, 0x228);
NW_ASSERT_SIZE(NCIcon, 0x22C);

#pragma pack(pop)