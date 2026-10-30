#pragma once
// NCProgressCtrl - count-down bar (UClass "CProgressCtrl", 0x1C0). While running, each paint
// adds the frame time (ms) to Pos; the bar shrinks from full to empty, and at Max the control
// stops and reports NWM_COMMAND(ID, 0) to the parent.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCProgressCtrl : public NControl
{
    NW_DECLARE_CLASS(NCProgressCtrl, NControl, 0, 0x102B96F0)

    UTexture* BarTex;          // +0x1F4 "NWindow.WhiteTexture" by default
    UTexture* BackTex;         // +0x1F8 "NWindow.BlackTexture" by default
    INT Unknown1A8;            // +0x1FC (1)   // C2: offset derived, no evidence
    INT Border;                // +0x200 bar inset (2)
    UBOOL bStopped;            // +0x204 (1: created stopped)
    DWORD Pos;                 // +0x208 ms elapsed
    DWORD Max;                 // +0x20C ms
    INT Unknown1BC;            // +0x210   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCProgressCtrl();                                                        // 0x100076f0
    NCProgressCtrl(const TCHAR* BackTexName, const TCHAR* BarTexName, INT InBorder); // 0x10007830
    // ~NCProgressCtrl                                                       // 0x100077e0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);          // 25 0x10021e60 (folded)
    virtual INT OnCreate() { return 1; }                                     // 37 0x100f7850 (folded)
    virtual INT OnDestroy() { return 1; }                                    // 38 0x100f7850 (folded)
    virtual INT OnPaint(NCGDevice* Device);                                  // 62 0x10007120

    // names ours
    void SetRemaining(DWORD Remaining);                                      // 0x10006e90
    void SetMax(DWORD InMax);                                                // 0x10006eb0
    void Reset();                                                            // 0x10006ec0
    void Stop();                                                             // 0x10006ee0
    void Start();                                                            // 0x10006ef0
};
NW_ASSERT_OFFSET(NCProgressCtrl, Border, 0x200);
NW_ASSERT_OFFSET(NCProgressCtrl, Max, 0x20C);
NW_ASSERT_SIZE(NCProgressCtrl, 0x214);

#pragma pack(pop)