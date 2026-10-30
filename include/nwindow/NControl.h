#pragma once
// NControl - base of every control (buttons, edit boxes, lists...) (UClass "Control", 0x1A0).
// A control reports to its parent with NWM_COMMAND (wParam = the control's ID, lParam = a code
// that depends on the control: NotifyID for buttons, 1/0 for press/release...).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NControl : public NCWnd
{
    NW_DECLARE_CLASS(NControl, NCWnd, 0, 0x102B8C88)

    INT ID;                    // +0xE8 set by the parent that creates the control
    UTexture* DisableTexture;  // +0xEC "NWindow.Disable" (OnCreate)
    INT NotifyID;              // +0xF0 id sent to the parent, -1 = ID (name ours)
    TCHAR Caption[128];        // +0xF4 (name ours)

    NControl();                                                     // 0x100030d0
    // ~NControl                                                    // 0x10001be0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual __declspec(noinline) INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam); // 25 0x10148f10 (folded with
                                                                    //    identical overrides)
    virtual INT OnCreate();                                         // 37 0x10006c50
    virtual INT OnDestroy();                                        // 38 0x10006c80
    virtual const TCHAR* GetValue() { return NULL; }                // 74 0x100023a0 (folded) (name
                                                                    //    ours) what an HTML form
                                                                    //    control holds

    // talking to the parent window (names ours)
    UBOOL NotifyParent(LPARAM Code);                                // 0x10006b30 NWM_COMMAND
    void ReleaseFocus();                                            // 0x10006b70
    void HideParent();                                              // 0x10006ba0
    UBOOL SendTo(NWndEntry* Target, UINT Msg, WPARAM wParam, LPARAM lParam); // 0x10006bd0
    UBOOL SendToParent(UINT Msg, WPARAM wParam, LPARAM lParam);    // 0x10006c10
    void SetCaption(const TCHAR* Text);                             // 0x10006d00
    const TCHAR* GetCaption();                                      // 0x10006d20

protected:
    NCWnd* ParentWnd() { return Entry->Parent ? Entry->Parent->Wnd : NULL; }
};
NW_ASSERT_OFFSET(NControl, ID, 0xE8);
NW_ASSERT_OFFSET(NControl, NotifyID, 0xF0);
NW_ASSERT_OFFSET(NControl, Caption, 0xF4);
NW_ASSERT_SIZE(NControl, 0x1F4);

#pragma pack(pop)