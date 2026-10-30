/*=============================================================================
    NCButton.cpp - NCButton, NCExButton, NCPushButton, NCRadioButton.
    Stock: 0x10001000 .. 0x10002550, 0x10003140 .. 0x10003b47, plus WindowProc 0x100051f0 / 0x100301f0 and
    OnDestroy 0x100189e0 (bodies the linker folded with other classes).
=============================================================================*/
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <wchar.h>

// interface sounds (FL2NetNotify::OnInterfacePlaySound)
enum { SOUND_Click = 0, SOUND_ClickDisabled = 2 };

// timer of the push button auto-repeat
enum { TIMER_PushRepeat = 0xFF000000 };

// 0x10032490 (NToolTip.cpp; anonymous in the db, name ours) - a tip whose text is a system string. NToolTipInfo's header has no such
// method: the stock function is written on a stand-in that is the same object
struct NToolTipInfoTextIDExt
{
    void SetTextID(INT InTextID, INT InStyle);
};

// The tool tip data's setters of this client: the text (NToolTipInfo::SetText, 0x10032460), the SysString 0x10032490 (it keeps the id at
// +0x18); both end with Style and Layout
static FORCEINLINE void ToolTipSetText(NToolTipInfo* Info, const TCHAR* InText, INT InStyle)
{
    Info->SetText(InText, InStyle);
}
static FORCEINLINE void ToolTipSetTextID(NToolTipInfo* Info, INT InID, INT InStyle)
{
    ((NToolTipInfoTextIDExt*)Info)->SetTextID(InID, InStyle);
}

/*-----------------------------------------------------------------------------
    NCButton.
-----------------------------------------------------------------------------*/

// 0x10003270 - no textures, label from Text, the tool tip below the button (NControl's constructor is inlined in the
// stock one: the same stores). Initialize does the rest
NCButton::NCButton()
{
    Initialize(NULL, NULL, NULL, 0);
}
NW_HOOK_CTOR(0x10003270, NCButton)

// 0x10003340 - the two textures by name; the last argument is Initialize's flag set
NCButton::NCButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey)
{
    Initialize(NormalTexName, PushedTexName, NULL, bInAcceptKey);
}
NW_HOOK_CTOR_ARGS(0x10003340, NCButton, (const TCHAR* A, const TCHAR* B, UBOOL C), (A, B, C))

// 0x10003410 (ret 0x10) - the three faces by name and the flags
NCButton::NCButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName, INT Flags)
{
    Initialize(NormalTexName, PushedTexName, OverTexName, Flags);
}
NW_HOOK_CTOR_ARGS(0x10003410, NCButton, (const TCHAR* A, const TCHAR* B, const TCHAR* C, INT D), (A, B, C, D))

// 0x100034e0 (ret 8; name ours) - the stock looks: the "btn1" faces at 76 x 23, the "smallbutton2" ones at 66 x 21, or the
// "btn1" faces left at the size the window has
NCButton::NCButton(INT Kind, INT Flags)
{
    if (Kind == 0)
    {
        Initialize(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), NULL, Flags);
        Width = 76.0f;
        Height = 23.0f;
    }
    else if (Kind == 1)
    {
        Initialize(TEXT("L2UI_CH3.Button.smallbutton2"), TEXT("L2UI_CH3.Button.smallbutton2_down"), NULL, Flags);
        Width = 66.0f;
        Height = 21.0f;
    }
    else if (Kind == 2)
        Initialize(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), NULL, Flags);
}
NW_HOOK_CTOR_ARGS(0x100034e0, NCButton, (INT A, INT B), (A, B))

// 0x10003140 - the label (+0x294) and the tool tip data (+0x214) go, then NControl's destructor (0x10003080)
NW_HOOK_DTOR(0x10003140, NCButton)

// 0x10001660 (ret 0x10) - the textures: normal and pushed by name; the over face by name, or (unless Flags has bit 1) as
// "<normal>_over" when the normal name is given and that one does not come
void NCButton::Initialize(const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName, INT Flags)
{
    bAcceptKey = Flags;
    bToolTipAbove = 1;
    bClickSound = 1;
    bMouseOver = 0;
    TextID = -1;
    ToolTipID = -1;
    TextOffsetX = 0;
    TextOffsetY = 0;
    ToolTipTex = FindTexture(TEXT("L2UI.tooltip.mini-tool01"), 1);
    NormalTex = NULL;
    PushedTex = NULL;
    OverTex = NULL;
    State = 0;
    if (NormalTexName)
        NormalTex = FindTexture(NormalTexName, 1);
    if (PushedTexName)
        PushedTex = FindTexture(PushedTexName, 1);
    if (!(bAcceptKey & 2))
    {
        if (OverTexName)
            OverTex = FindTexture(OverTexName, 1);
        if (NormalTexName && !OverTex)
        {
            FString OverName = FString::Printf(TEXT("%s_over"), NormalTexName);
            OverTex = FindTexture(*OverName, 0);
        }
    }
}
NW_HOOK_METHOD(0x10001660, NCButton, Initialize)

// 0x10021e60 (vslot 25) - one body for the buttons (C2 dropped the enabled test)
INT NCButton::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10021e60, NCButton, WindowProc)

// 0x100013b0 (vslot 38) - C2 guards it and empties the label
INT NCButton::OnDestroy()
{
    guard(NCButton::OnDestroy);
    NControl::OnDestroy();
    Text.Empty();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100013b0, NCButton, OnDestroy)

// 0x10001000 (guard) - the face the State picks (0 normal, 1 pressed, 2 over: the normal one when there is none, any other
// state draws no face), from the clip offset so a button clipped by its parent is cut, not squeezed; the label (the
// SysString, or Text) centered, grey when disabled. Width and Height are FLOATs; the label is placed with the halves
// in floating point
void NCButton::DrawButton(NCGDevice* Device)
{
    guard(NCButton::DrawButton);
    UTexture* Face = NULL;
    switch (State)
    {
    case 0:
        Face = NormalTex;
        break;
    case 1:
        Face = PushedTex;
        break;
    case 2:
        Face = OverTex;
        if (!Face)
            Face = NormalTex;
        break;
    }
    FString Label;
    if (TextID >= 0)
        Label = *SysString(TextID);
    else
        Label = Text;
    DWORD Color = IsEnabled() ? 0xFFE6DCBE : 0xFFA0A0A0;
    if (Face)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, ClipOffsetX, ClipOffsetY, (INT)Width, (INT)Height, Face, 0xFF, 1);
    if (Label.Num() && *Label.GetCharData())
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *Label, 0, 0, 0, 0, 0, L2FONT_Normal);
        INT Y = (INT)((DOUBLE)TextOffsetY + (DOUBLE)Height * 0.5 - (DOUBLE)(Size.cy / 2));
        INT X = (INT)((DOUBLE)TextOffsetX + (DOUBLE)Width * 0.5 - (DOUBLE)(Size.cx / 2));
        Device->DrawText(X, Y, Color, *Label, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    unguard;
}
NW_HOOK_METHOD(0x10001000, NCButton, DrawButton)

// 0x10002410 (vslot 62) - the clip of the button, DrawButton in it
INT NCButton::OnPaint(NCGDevice* Device)
{
    guard(NCButton::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    DrawButton(Device);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10002410, NCButton, OnPaint)

// 0x10003610 (vslot 63) - the tool tip is the window skin's (the button's ToolTip by value, drawn at the window's
// corner: X and Y are 0 now). C3 measures nothing: the local rectangle of the desktop is asked for and not used
INT NCButton::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bMouseOver)
    {
        NRectF Screen;
        Screen.X = 0;
        Screen.Y = 0;
        Screen.W = 0.0f;
        Screen.H = 0.0f;
        GConsole->Desktop->GetLocalRect(&Screen);
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            ToolTipSkin->Draw(Device, ToolTip, 0, 0, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10003610, NCButton, OnDrawToolTip)

// 0x10001440 (vslot 65) - Enter / Space click it while it has the focus (C3: the sound only with bClickSound)
INT NCButton::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if ((wParam == VK_RETURN || wParam == VK_SPACE) && bAcceptKey == 1)
    {
        if (bClickSound)
            PlayInterfaceSound(IsEnabled() ? SOUND_Click : SOUND_ClickDisabled);
        NotifyParent(0);
    }
    return 1;
}
NW_HOOK_METHOD(0x10001440, NCButton, OnKeyDown)

// 0x100012e0 (vslot 67, guard) - C4: over the button (the top window under the pointer) it shows the over face, or the
// pressed one while the left button is down, and takes the mouse (SetCapture, slot 58); elsewhere it lets go (59)
INT NCButton::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCButton::OnMouseMove);
    if (CallIsTopWindowAt(LOWORD(lParam), HIWORD(lParam)))
    {
        bMouseOver = 1;
        State = (wParam & MK_LBUTTON) ? 1 : 2;
        SetCapture();
    }
    else
    {
        State = 0;
        bMouseOver = 0;
        ReleaseCapture();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100012e0, NCButton, OnMouseMove)

// 0x10001200 (vslot 68) - C3: the click sound is optional (bClickSound)
INT NCButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (IsInWindow(LOWORD(lParam), HIWORD(lParam)))
    {
        State = 1;
        if (bClickSound)
            PlayInterfaceSound(IsEnabled() ? SOUND_Click : SOUND_ClickDisabled);
    }
    return 1;
}
NW_HOOK_METHOD(0x10001200, NCButton, OnLButtonDown)

// 0x10001270 (vslot 69) - the click: released inside, enabled; then the face of where the pointer is
INT NCButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (IsInWindow(X, Y) && IsEnabled())
        NotifyParent(NotifyID == -1 ? ID : NotifyID);
    State = CallIsTopWindowAt(X, Y) ? 2 : 0;
    return 1;
}
NW_HOOK_METHOD(0x10001270, NCButton, OnLButtonUp)

// 0x100014a0 (name ours) - the label is the SysString (copied into Text too)
void NCButton::SetTextID(INT InID)
{
    if (*SysString(InID))
    {
        TextID = InID;
        Text = *SysString(InID);
    }
}
NW_HOOK_METHOD(0x100014a0, NCButton, SetTextID)

// 0x10001500 - C2: Text is an FString
void NCButton::SetText(const TCHAR* InText)
{
    if (InText && *InText)
    {
        TextID = -1;
        Text = InText;
    }
}
NW_HOOK_METHOD(0x10001500, NCButton, SetText)

// 0x10001530
UBOOL NCButton::SetNormalTex(const TCHAR* Name)
{
    if (Name && *Name)
    {
        NormalTex = FindTexture(Name, 1);
        return 1;
    }
    NormalTex = NULL;
    return 1;
}
NW_HOOK_METHOD(0x10001530, NCButton, SetNormalTex)

// 0x10001570 - unlike SetNormalTex, an empty name still looks the texture up
UBOOL NCButton::SetPushedTex(const TCHAR* Name)
{
    if (Name)
    {
        PushedTex = FindTexture(Name, 1);
        return 1;
    }
    PushedTex = NULL;
    return 1;
}
NW_HOOK_METHOD(0x10001570, NCButton, SetPushedTex)

// 0x100015b0 - the tip's text is a SysString; the tool tip data gets it as a plain tip (NTT_Plain)
void NCButton::SetToolTip(INT InID, UBOOL bAbove)
{
    if (InID >= 0)
    {
        if (*SysString(InID))
        {
            ToolTipID = InID;
            bToolTipAbove = bAbove;
            ToolTipSetTextID(&ToolTip, InID, NTT_Plain);
        }
    }
}
NW_HOOK_METHOD(0x100015b0, NCButton, SetToolTip)

// 0x10001600 (ret 8; anonymous in the db) - the same with a text: nothing for NULL, else the tip's side and the text as a
// plain tip (the stock code tail-jumps into the tool tip data's SetText with the style 0x400)
void NCButton::SetToolTipText(const TCHAR* InText, UBOOL bAbove)
{
    if (InText)
    {
        bToolTipAbove = bAbove;
        ToolTipSetText(&ToolTip, InText, NTT_Plain);
    }
}
NW_HOOK_METHOD(0x10001600, NCButton, SetToolTipText)

NW_CHECK_VTABLE(NCButton, 0x1019eb10, 75)

/*-----------------------------------------------------------------------------
    NCExButton: reports press (1) and release (0).
-----------------------------------------------------------------------------*/

// 0x100036f0 - NCButton's, only the vtable differs
NCExButton::NCExButton()
{
}
NW_HOOK_CTOR(0x100036f0, NCExButton)

// 0x10003790
NCExButton::NCExButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey)
    : NCButton(NormalTexName, PushedTexName, bInAcceptKey)
{
}
NW_HOOK_CTOR_ARGS(0x10003790, NCExButton, (const TCHAR* A, const TCHAR* B, UBOOL C), (A, B, C))

// 0x10003740
NW_HOOK_DTOR(0x10003740, NCExButton)

// 0x100e9f60 (vslot 25) - one body for NCExButton, NCPushButton and NCRadioButton
INT NCExButton::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCButton::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x100e9f60, NCExButton, WindowProc)

// 0x100017d0 (vslot 68) - reports even when the press is outside the button
INT NCExButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (IsInWindow(LOWORD(lParam), HIWORD(lParam)))
        State = 1;
    NotifyParent(1);
    return 1;
}
NW_HOOK_METHOD(0x100017d0, NCExButton, OnLButtonDown)

// 0x10001810 (vslot 69)
INT NCExButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    NotifyParent(0);
    State = 0;
    return 1;
}
NW_HOOK_METHOD(0x10001810, NCExButton, OnLButtonUp)

NW_CHECK_VTABLE(NCExButton, 0x1019ef00, 75)

/*-----------------------------------------------------------------------------
    NCPushButton: reports on press, then every RepeatDelay ms while held.
-----------------------------------------------------------------------------*/

// 0x10003890
NCPushButton::NCPushButton()
{
    bHeld = 0;
    RepeatDelay = 100;
    bAutoRepeat = 1;
}
NW_HOOK_CTOR(0x10003890, NCPushButton)

// 0x10003940 - RepeatDelay and bAutoRepeat stay as allocated (zero): no auto-repeat
NCPushButton::NCPushButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
{
    bHeld = 0;
    SetNormalTex(NormalTexName);
    SetPushedTex(PushedTexName);
}
NW_HOOK_CTOR_ARGS(0x10003940, NCPushButton, (const TCHAR* A, const TCHAR* B), (A, B))

// 0x100038f0
NW_HOOK_DTOR(0x100038f0, NCPushButton)

// vslot 25: the stock slot holds NCExButton's body above (0x100e9f60, one body for the three)
INT NCPushButton::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCButton::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100019a0 (vslot 36)
INT NCPushButton::OnTimer(INT TimerID)
{
    if (TimerID == (INT)TIMER_PushRepeat && bHeld)
        NotifyParent(0);
    return 1;
}
NW_HOOK_METHOD(0x100019a0, NCPushButton, OnTimer)

// 0x100019d0 (vslot 38) - guarded; NCButton's (the stock one calls it, 0x100013b0: no inline copy)
INT NCPushButton::OnDestroy()
{
    guard(NCPushButton::OnDestroy);
    NCButton::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100019d0, NCPushButton, OnDestroy)

// 0x10001940 (vslot 67)
INT NCPushButton::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if ((wParam & MK_LBUTTON) && IsFocused() && IsInWindow(LOWORD(lParam), HIWORD(lParam)))
    {
        bHeld = 1;
        State = 1;
        return 1;
    }
    bHeld = 0;
    State = 0;
    return 1;
}
NW_HOOK_METHOD(0x10001940, NCPushButton, OnMouseMove)

// 0x10001880 (vslot 68) - no hit test; NotifyID is sent as is (-1 is not replaced by ID); the sound is optional
INT NCPushButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    State = 1;
    bHeld = 1;
    if (bClickSound)
        PlayInterfaceSound(IsEnabled() ? SOUND_Click : SOUND_ClickDisabled);
    NotifyParent(NotifyID);
    if (bAutoRepeat)
        SetTimer(TIMER_PushRepeat, RepeatDelay, NULL);
    return 1;
}
NW_HOOK_METHOD(0x10001880, NCPushButton, OnLButtonDown)

// 0x10001900 (vslot 69)
INT NCPushButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    State = 0;
    bHeld = 0;
    if (bAutoRepeat)
        KillTimer(TIMER_PushRepeat);
    ReleaseFocus();
    return 1;
}
NW_HOOK_METHOD(0x10001900, NCPushButton, OnLButtonUp)

NW_CHECK_VTABLE(NCPushButton, 0x1019f160, 75)

/*-----------------------------------------------------------------------------
    NCRadioButton: the face follows bChecked (on = pressed, hover while off = over).
-----------------------------------------------------------------------------*/

// 0x100039a0 - bChecked is left as allocated
NCRadioButton::NCRadioButton()
{
    CreateHeight = 0;
    CreateWidth = 0;
}
NW_HOOK_CTOR(0x100039a0, NCRadioButton)

// 0x100039c0 (anonymous in the db) - sets the textures twice (base constructor, then the setters), as the stock code
NCRadioButton::NCRadioButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
    : NCButton(NormalTexName, PushedTexName, 0)
{
    CreateHeight = 0;
    CreateWidth = 0;
    bChecked = 0;
    SetNormalTex(NormalTexName);
    SetPushedTex(PushedTexName);
}
NW_HOOK_CTOR_ARGS(0x100039c0, NCRadioButton, (const TCHAR* A, const TCHAR* B), (A, B))

// 0x10001a50 (anonymous in the db, ret 4; name ours) - the tab control's way to set a tab: released (1) is off, pressed (0) is on
void NCRadioButton::SetReleased(UBOOL bInReleased)
{
    UBOOL bOn = (bInReleased == 0);
    bChecked = bOn;
    State = (bOn != 0);
}
NW_HOOK_METHOD(0x10001a50, NCRadioButton, SetReleased)

// 0x100031f0
NW_HOOK_DTOR(0x100031f0, NCRadioButton)

// vslot 25: the stock slot holds NCButton's body (0x10021e60, one body for the three)
INT NCRadioButton::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCButton::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10001eb0 (vslot 37) - remembers the size it was created with
INT NCRadioButton::OnCreate()
{
    NControl::OnCreate();
    CreateWidth = (INT)Width;
    CreateHeight = (INT)Height;
    return 1;
}
NW_HOOK_METHOD(0x10001eb0, NCRadioButton, OnCreate)

// 0x10001ee0 (vslot 38) - NCButton's, a bare call
INT NCRadioButton::OnDestroy()
{
    NCButton::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10001ee0, NCRadioButton, OnDestroy)

// 0x100024e0 (vslot 63) - the tip covers the button itself, sized to its text (the plain NCButton draws through the
// skin). C3 asks for the desktop's local rectangle and does not use it; the text is light grey
INT NCRadioButton::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (ToolTipID >= 0)
    {
        if (*SysString(ToolTipID) && bMouseOver)
        {
            SIZE Size;
            NCGDevice::GetTextExtent(&Size, *SysString(ToolTipID), 0, 0, 0, 0, 0, L2FONT_Normal);
            NRectF Screen;
            Screen.X = 0;
            Screen.Y = 0;
            Screen.W = 0.0f;
            Screen.H = 0.0f;
            GConsole->Desktop->GetLocalRect(&Screen);
            Device->SetClip(ScreenX, ScreenY, Size.cx, Size.cy);
            if (ToolTipTex)
                Device->DrawStretchedTex(0, 0, Size.cx, Size.cy, ToolTipTex, 1, 0xFF);
            Device->DrawText(0, 0, 0xFFDCDCDC, *SysString(ToolTipID), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            Device->ReleaseClip();
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100024e0, NCRadioButton, OnDrawToolTip)

// 0x10001b90 (vslot 67, guard) - only while enabled: the face of the state (on = pressed; over while off and the top window
// under the pointer, which also takes the mouse) and whether the pointer is inside
INT NCRadioButton::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCRadioButton::OnMouseMove);
    if (IsEnabled())
    {
        INT X = LOWORD(lParam);
        INT Y = HIWORD(lParam);
        if (CallIsTopWindowAt(X, Y))
        {
            State = (bChecked == 0) ? 2 : 1;
            SetCapture();
        }
        else
        {
            State = (bChecked != 0) ? 1 : 0;
            ReleaseCapture();
        }
        bMouseOver = PtInRect(X, Y, Rect);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10001b90, NCRadioButton, OnMouseMove)

// 0x10001a80 (vslot 68, guard) - C4: nothing but the enabled test, whose answer is dropped
INT NCRadioButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    guard(NCRadioButton::OnLButtonDown);
    UBOOL bNow = IsEnabled();
    (void)bNow;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10001a80, NCRadioButton, OnLButtonDown)

// 0x10001b00 (vslot 69, guard) - only while enabled
INT NCRadioButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    guard(NCRadioButton::OnLButtonUp);
    if (IsEnabled())
        ReleaseFocus();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10001b00, NCRadioButton, OnLButtonUp)

NW_CHECK_VTABLE(NCRadioButton, 0x1019ec40, 75)
