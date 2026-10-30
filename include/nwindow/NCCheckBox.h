#pragma once
// NCCheckBox - check box with a label (UClass "CCheckBox", 0x2C4). A click toggles bChecked
// and reports it to the parent (NWM_COMMAND, lParam = new state), unless bDisabled.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCCheckBox : public NControl
{
    NW_DECLARE_CLASS(NCCheckBox, NControl, 0, 0x102B7CE8)

    // C4 layout (the constructors 0x10005720 / 0x100058c0, OnCreate 0x10005230, OnPaint 0x100053b0): the window has a tool tip
    // of its own (an NToolTipInfo, 0x80 bytes) and the label can be a SysString (LabelID)
    NToolTipInfo ToolTip;            // +0x1F4 (name ours) drawn through the window's skin by OnPostPaint (slot 63)
    UTexture* CheckedTex;            // +0x274 "L2UI.Control.CheckBox_Checked"
    UTexture* UncheckedTex;          // +0x278 "L2UI.Control.CheckBox"
    UTexture* UncheckedDisabledTex;  // +0x27C "L2UI.Control.CheckBox_Unable"
    UTexture* CheckedDisabledTex;    // +0x280 "L2UI.Control.CheckBox_Checked_unable"
    FString Label;                   // +0x284 (C1: TCHAR Label[128])
    INT LabelID;                     // +0x290 (name ours) SysString index of the label, -1 = Label
    UBOOL bChecked;                  // +0x294
    UBOOL bDisabled;                 // +0x298 grey, clicks ignored
    UBOOL bLabelLeft;                // +0x29C label first, box after it
    INT BoxW;                        // +0x2A0 (12)
    INT BoxH;                        // +0x2A4 (12)
    DWORD TextParam;                 // +0x2A8 new in C2 (name ours): the constructor's fourth argument, which the label is
                                     //   drawn with (NCGDevice::DrawText's fifth argument)

    NCCheckBox(const TCHAR* InLabel = TEXT("default"), UBOOL bInChecked = 0,
               UBOOL bInLabelLeft = 0, INT InTextParam = 0);             // 0x10005720
    NCCheckBox(INT InLabelID, UBOOL bInChecked, UBOOL bInLabelLeft, INT InTextParam = 0);   // 0x100058c0 SysString (ret 0x10)
    // ~NCCheckBox                                                       // 0x10005840

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);      // 25 0x10090b60
    virtual INT OnCreate();                                              // 37 0x10005230
    virtual INT OnDestroy();                                             // 38 0x10005300
    virtual INT OnPaint(NCGDevice* Device);                              // 62 0x100053b0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);             // 68 0x10005380
    virtual INT OnDrawToolTip(NCGDevice* Device) ;  // 63 0x10005990 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    // 0x10005990 (the db says OnPostPaint): the body of slot 63, hooked by its address under this name because the slot's
    // own definition is the generated stub
    INT OnPostPaint(NCGDevice* Device);
};
NW_ASSERT_OFFSET(NCCheckBox, ToolTip, 0x1F4);
NW_ASSERT_OFFSET(NCCheckBox, CheckedTex, 0x274);
NW_ASSERT_OFFSET(NCCheckBox, Label, 0x284);
NW_ASSERT_OFFSET(NCCheckBox, LabelID, 0x290);
NW_ASSERT_OFFSET(NCCheckBox, bChecked, 0x294);
NW_ASSERT_OFFSET(NCCheckBox, BoxH, 0x2A4);
NW_ASSERT_SIZE(NCCheckBox, 0x2AC);

#pragma pack(pop)