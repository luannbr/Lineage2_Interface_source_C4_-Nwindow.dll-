#pragma once
// NCButton and its subclasses (UClass "CButton" 0x21C, "CExButton", "CPushButton",
// "CRadioButton").
//
// A button paints NormalTex, PushedTex while it is pressed or OverTex while the mouse is over it (State), over its rectangle,
// with its label centered (a SysString index, or Text). Clicks play interface sound 0 (2 when
// disabled) and report NWM_COMMAND(ID, NotifyID) to the parent on release inside the button.
// The tool tip (ToolTipID, a SysString index) is drawn above the button, or below it, while
// the mouse is over it.
//   NCExButton     reports 1 on press and 0 on release instead.
//   NCPushButton   reports on press and, while held, again every RepeatDelay ms (timer).
//   NCRadioButton  stays down once clicked; reports Value.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton : public NControl
{
    NW_DECLARE_CLASS(NCButton, NControl, 0, 0x102B5DB8)

    // C4 layout (Initialize 0x10001660, the constructors 0x10003270 / 0x10003340, DrawButton 0x10001000): the pressed flag
    // became a state with a hover face; the texture sizes are gone; the tool tip data is an NToolTipInfo (0x80 bytes)
    // at +0x214 (the C3 header had a 0x74-byte filler at +0x220).
    union
    {
        INT State;             // +0x1F4 0 normal, 1 pressed, 2 mouse over (DrawButton picks NormalTex, PushedTex, OverTex)
        UBOOL bReleased;       // the C3 name (0 = pressed there); other windows' code that still says bReleased reads State
    };
    UBOOL bMouseOver;          // +0x1F8 set by OnMouseMove, shows the tool tip
    UBOOL bToolTipAbove;       // +0x1FC 1 = tool tip above the button, 0 = below
    UTexture* NormalTex;       // +0x200
    UTexture* PushedTex;       // +0x204
    UTexture* OverTex;         // +0x208 (name ours) new in C4: the third face, the constructor's "<normal>_over" texture
    UTexture* ToolTipTex;      // +0x20C "L2UI.tooltip.mini-tool01", stretched under the tip (NCRadioButton's tool tip)
    INT bAcceptKey;            // +0x210 Initialize's last argument, a flag set: bit 0 = Enter / Space click it while it has
                               //   the focus (OnKeyDown tests == 1), bit 1 = do not look for an over texture
    NToolTipInfo ToolTip;      // +0x214 the tool tip's data, filled by SetToolTip; OnDrawToolTip hands a copy of it to
                               //   the window's tool tip skin (NCWnd::ToolTipSkin)
    FString Text;              // +0x294 label when TextID < 0 (C1: TCHAR Text[32])
    INT TextID;                // +0x2A0 SysString index of the label, -1 = Text
    INT ToolTipID;             // +0x2A4 SysString index of the tool tip, -1 = none
    INT TextOffsetX;           // +0x2A8 label offset from the center
    INT TextOffsetY;           // +0x2AC
    UBOOL bClickSound;         // +0x2B0 (name ours) new in C3: 1 (every constructor): the click sounds (OnLButtonDown, OnKeyDown,
                               //   NCPushButton::OnLButtonDown) play only while it is set

    // out of line: the stock subclasses' constructors call these (inlined, they would construct the label twice)
    __declspec(noinline) NCButton();                                            // 0x10003270
    __declspec(noinline) NCButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey); // 0x10003340
    // C4: all three faces by name (the over one may be NULL: Initialize derives it) and Initialize's flags
    __declspec(noinline) NCButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName, INT Flags); // 0x10003410 (ret 0x10)
    // C4: the stock button looks (name ours): Kind 0 the 76 x 23 "btn1" one, 1 the small 66 x 21 one, 2 the "btn1" faces at the
    // size the button has; Flags are Initialize's
    __declspec(noinline) NCButton(INT Kind, INT Flags);                                      // 0x100034e0 (ret 8)
    // ~NCButton                                                                // 0x10003140

    virtual __declspec(noinline) INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam); // 25 0x10021e60
    virtual INT OnDestroy();                                                    // 38 0x100013b0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                                     // 62 0x10002410
    virtual INT OnDrawToolTip(NCGDevice* Device);                               // 63 0x10003610 (the db says OnPostPaint)
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);                        // 65 0x10001440
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);                      // 67 0x100012e0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);                    // 68 0x10001200
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                      // 69 0x10001270

    // 0x10001660 (ret 0x10; named by the db): what both constructors share. Over is looked up by name, or, with the
    // flag 2 clear and a Normal name, as "<Normal>_over".
    __declspec(noinline) void Initialize(const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName,
                                         INT Flags);
    // 0x10001000 (db name from its guard): the face for the State and the label, in the clip OnPaint set
    __declspec(noinline) void DrawButton(NCGDevice* Device);

    // names ours
    void SetTextID(INT ID);                                                     // 0x100014a0
    void SetText(const TCHAR* InText);                                          // 0x10001500
    __declspec(noinline) UBOOL SetNormalTex(const TCHAR* Name);                                      // 0x10001530
    __declspec(noinline) UBOOL SetPushedTex(const TCHAR* Name);                                      // 0x10001570
    void SetToolTip(INT ID, UBOOL bAbove);                                      // 0x100015b0
    // 0x10001600 (ret 8; anonymous in the db): a tip with a text of its own (the party members' class icons), not a SysString
    void SetToolTipText(const TCHAR* InText, UBOOL bAbove);
};
NW_ASSERT_OFFSET(NCButton, State, 0x1F4);
NW_ASSERT_OFFSET(NCButton, NormalTex, 0x200);
NW_ASSERT_OFFSET(NCButton, OverTex, 0x208);
NW_ASSERT_OFFSET(NCButton, bAcceptKey, 0x210);
NW_ASSERT_OFFSET(NCButton, ToolTip, 0x214);
NW_ASSERT_OFFSET(NCButton, Text, 0x294);
NW_ASSERT_OFFSET(NCButton, TextID, 0x2A0);
NW_ASSERT_OFFSET(NCButton, TextOffsetY, 0x2AC);
NW_ASSERT_OFFSET(NCButton, bClickSound, 0x2B0);
NW_ASSERT_SIZE(NCButton, 0x2B4);

class NCExButton : public NCButton
{
    NW_DECLARE_CLASS(NCExButton, NCButton, 0, 0x102B62E8)

    NCExButton();                                                               // 0x100036f0
    NCExButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey); // 0x10003790
    // ~NCExButton                                                              // 0x10003740

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);             // 25 0x100e9f60 (folded)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);                    // 68 0x100017d0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                      // 69 0x10001810
};
NW_ASSERT_SIZE(NCExButton, 0x2B4);

class NCPushButton : public NCButton
{
    NW_DECLARE_CLASS(NCPushButton, NCButton, 0, 0x102B4E20)

    UBOOL bHeld;               // +0x2B4 pressed and still inside
    INT RepeatDelay;           // +0x2B8 ms between repeats (100)
    UBOOL bAutoRepeat;         // +0x2BC (1)

    NCPushButton();                                                             // 0x10003890
    NCPushButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName);       // 0x10003940
    // ~NCPushButton                                                            // 0x100038f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);             // 25 0x100e9f60 (folded)
    virtual INT OnTimer(INT TimerID);                                           // 36 0x100019a0
    virtual INT OnDestroy();                                                    // 38 0x100019d0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);                      // 67 0x10001940
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);                    // 68 0x10001880
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                      // 69 0x10001900
};
NW_ASSERT_OFFSET(NCPushButton, bHeld, 0x2B4);
NW_ASSERT_SIZE(NCPushButton, 0x2C0);

class NCRadioButton : public NCButton
{
    NW_DECLARE_CLASS(NCRadioButton, NCButton, 0, 0x102B5880)

    // C4 (the constructors 0x100039a0 / 0x100039c0, OnCreate 0x10001eb0, OnMouseMove 0x10001b90): the C3 Value is gone;
    // the size at OnCreate stays; a third field says whether the button is on (the face: on = pressed, hover while off =
    // over); the default constructor leaves it as allocated
    INT CreateWidth;           // +0x2B4 (INT)Width at OnCreate
    INT CreateHeight;          // +0x2B8 (INT)Height at OnCreate
    UBOOL bChecked;            // +0x2BC (name ours) on

    NCRadioButton();                                                            // 0x100039a0
    NCRadioButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName);      // 0x100039c0 (anonymous in the db)
    void SetReleased(UBOOL bInReleased);                                        // 0x10001a50 (anonymous in the db, ret 4, name ours) 1 = off
    // ~NCRadioButton                                                           // 0x100031f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);             // 25 0x10021e60 (folded)
    virtual INT OnCreate();                                                     // 37 0x10001eb0
    virtual INT OnDestroy();                                                    // 38 0x10001ee0 (folded)
    virtual INT OnDrawToolTip(NCGDevice* Device);                               // 63 0x100024e0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);                      // 67 0x10001b90
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);                    // 68 0x10001a80
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                      // 69 0x10001b00
};
NW_ASSERT_OFFSET(NCRadioButton, CreateWidth, 0x2B4);
NW_ASSERT_OFFSET(NCRadioButton, bChecked, 0x2BC);
NW_ASSERT_SIZE(NCRadioButton, 0x2C0);

#pragma pack(pop)