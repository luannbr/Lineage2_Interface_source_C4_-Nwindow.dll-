/*=============================================================================
    NCFrameCtrl.cpp - title bar and resize edge. Stock (C3): 0x1000f6d0 .. 0x100106a0.
=============================================================================*/
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NClasses.gen.h"

#include <wchar.h>

// interface sound played when a window is closed from its title bar
enum { SOUND_WindowClose = 6 };

/*-----------------------------------------------------------------------------
    NCFrameCtrl.
-----------------------------------------------------------------------------*/

// 0x10012560 - NControl's constructor is inlined; the bar of a decorated window (the default constructor): the nine textures
// by name, the title a SysString index of -1. The shape flags (bVertical, bSmall) and the expand button's heights are left
// as allocated
NCFrameCtrl::NCFrameCtrl()
{
    TitleID = -1;
    CloseButton = NULL;
    MinimizeButton = NULL;
    LastMouseY = 0;
    LastMouseX = 0;
    TitleY = 0;
    TitleX = 0;
    GrabY = 0;
    GrabX = 0;
    BackLeftTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackLeft"), 1);
    BackMidTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackMid"), 1);
    BackRightTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackRight"), 1);
    SmallBarTex[0] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h1"), 1);
    SmallBarTex[1] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h2"), 1);
    SmallBarTex[2] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h3"), 1);
    SmallBarTex[3] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar1"), 1);
    SmallBarTex[4] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar2"), 1);
    SmallBarTex[5] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar3"), 1);
    bMoved = 0;
    Expand.Button = NULL;
    Expand.bExpanded = 0;
    bDecorated = 1;
}
NW_HOOK_CTOR(0x10012560, NCFrameCtrl)

// 0x100126e0 (ret 0xc) - the same with the three flags (the title id is not set here: it stays as allocated)
NCFrameCtrl::NCFrameCtrl(INT InDecorated, INT InVertical, INT InSmall)
{
    CloseButton = NULL;
    MinimizeButton = NULL;
    LastMouseY = 0;
    LastMouseX = 0;
    TitleY = 0;
    TitleX = 0;
    GrabY = 0;
    GrabX = 0;
    BackLeftTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackLeft"), 1);
    BackMidTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackMid"), 1);
    BackRightTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.FrameBackRight"), 1);
    SmallBarTex[0] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h1"), 1);
    SmallBarTex[1] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h2"), 1);
    SmallBarTex[2] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar_h3"), 1);
    SmallBarTex[3] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar1"), 1);
    SmallBarTex[4] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar2"), 1);
    SmallBarTex[5] = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar3"), 1);
    bDecorated = InDecorated;
    bMoved = 0;
    Expand.Button = NULL;
    Expand.bExpanded = 0;
    bVertical = InVertical;
    bSmall = InSmall;
}
NW_HOOK_CTOR_ARGS(0x100126e0, NCFrameCtrl, (INT A, INT B, INT C), (A, B, C))

// 0x100124c0 - ConditionalDestroy, ~FString (Title), ~NControl
NW_HOOK_DTOR(0x100124c0, NCFrameCtrl)

// 0x100115b0 (vslot 25) - the buttons report here: minimize (slot 77), close (76), and the expand button of the target
// window (slot 78: OnExpandBtnClick)
INT NCFrameCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_FRAME_MINIMIZE)
            return OnMinimize() != 0;
        if (wParam == NID_FRAME_CLOSE)
            return OnClose() != 0;
        if (wParam == NID_FRAME_EXPAND)
            return ::nw::VirtualCall<INT>(this, 78) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100115b0, NCFrameCtrl, WindowProc)

// A button of the bar: the stock OnCreate allocates it through NCButton's operator new out of line (0x10005d90, cdecl:
// StaticAllocateObject of the class, which is what the class's `new` does) and runs the constructor on the block
static FORCEINLINE NCButton* NewFrameButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
{
    return new NCButton(NormalTexName, PushedTexName, 0);
}

// 0x100121c0 (vslot 37) - the buttons of a decorated frame, right-aligned (15x15 at y 3, the CH3 button textures)
INT NCFrameCtrl::OnCreate()
{
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    GetLocalRect(&R);
    if (bDecorated && Entry && Entry->Parent && Entry->Parent->Wnd)
    {
        if (Entry->Parent->Wnd->GetStyle() & NWS_MINIMIZABLE)
        {
            MinimizeButton = NewFrameButton(TEXT("L2UI_CH3.FrameCtrl.FrameMiniBtn"),
                                            TEXT("L2UI_CH3.FrameCtrl.FrameMiniOnBtn"));
            MinimizeButton->ID = NID_FRAME_MINIMIZE;
            MinimizeButton->SetPosition((INT)(Width - 40.0f), 3, 15, 15);
            MinimizeButton->AttachEntry(AddChildWindow(MinimizeButton, NWS_VISIBLE));
        }
        if (!(Entry->Parent->Wnd->GetStyle() & NWS_NOCLOSE))
        {
            CloseButton = NewFrameButton(TEXT("L2UI_CH3.FrameCtrl.FrameCloseBtn"),
                                         TEXT("L2UI_CH3.FrameCtrl.FrameCloseOnBtn"));
            CloseButton->ID = NID_FRAME_CLOSE;
            CloseButton->SetPosition((INT)(Width - 23.0f), 3, 15, 15);
            CloseButton->AttachEntry(AddChildWindow(CloseButton, NWS_VISIBLE));
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100121c0, NCFrameCtrl, OnCreate)

// 0x100119b0 (vslot 38, guard) - the buttons go: minimize, close, expand
INT NCFrameCtrl::OnDestroy()
{
    guard(NCFrameCtrl::OnDestroy);
    if (MinimizeButton)
    {
        RemoveChildWindow(MinimizeButton->Entry);
        delete MinimizeButton;
        MinimizeButton = NULL;
    }
    if (CloseButton)
    {
        RemoveChildWindow(CloseButton->Entry);
        delete CloseButton;
        CloseButton = NULL;
    }
    if (Expand.Button)
    {
        RemoveChildWindow(Expand.Button->Entry);
        delete Expand.Button;
        Expand.Button = NULL;
    }
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100119b0, NCFrameCtrl, OnDestroy)

// 0x10012380 (ret 8, guard) - the expand button of the target window, made once: (1, 6), 11 x 13, the info-button texture
void NCFrameCtrl::CreateExpandBtn(INT InCollapsedH, INT InExpandedH)
{
    guard(NCFrameCtrl::CreateExpandBtn);
    Expand.CollapsedH = InCollapsedH;
    Expand.ExpandedH = InExpandedH;
    if (!Expand.Button)
    {
        Expand.Button = new NCButton(TEXT("L2UI_CH3.TargetWnd.Target_InfoButton"),
                                    TEXT("L2UI_CH3.TargetWnd.Target_InfoButton"), 0);
        Expand.Button->ID = NID_FRAME_EXPAND;
        Expand.Button->SetPosition(1, 6, 11, 13);
        CreateChild(Expand.Button, NWS_VISIBLE);
    }
    unguard;
}
NW_HOOK_METHOD(0x10012380, NCFrameCtrl, CreateExpandBtn)

// 0x10011ab0 (vslot 78, guard) - the expand button was clicked: the button's picture, the parent window (SetWindowSize,
// slot 47) and this bar (SetFrameSize, slot 75) take the other height, and the owner is told (0xFFFF now expanded, 0xFFFE
// not)
INT NCFrameCtrl::OnExpandBtnClick()
{
    guard(NCFrameCtrl::OnExpandBtnClick);
    NCWnd* ParentWnd = Entry->Parent->Wnd;
    FLOAT ParentW = ParentWnd->Width;
    INT NewH;
    if (Expand.bExpanded)
    {
        Expand.Button->SetNormalTex(TEXT("L2UI_CH3.TargetWnd.Target_InfoButton"));
        Expand.Button->SetPushedTex(TEXT("L2UI_CH3.TargetWnd.Target_InfoButton"));
        NewH = Expand.CollapsedH;
    }
    else
    {
        Expand.Button->SetNormalTex(TEXT("L2UI_CH3.TargetWnd.Target_InfoButton_Down"));
        Expand.Button->SetPushedTex(TEXT("L2UI_CH3.TargetWnd.Target_InfoButton_Down"));
        NewH = Expand.ExpandedH;
    }
    ParentWnd->SetWindowSize(ParentW, (FLOAT)NewH);
    SetFrameSize((DWORD)(INT)Width, (DWORD)NewH);
    Expand.bExpanded = (Expand.bExpanded == 0);
    NotifyParent(Expand.bExpanded ? 0xFFFF : 0xFFFE);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10011ab0, NCFrameCtrl, OnExpandBtnClick)

// 0x10011c00 (vslot 62) - the bar is 0x14 high with 16 px caps (or the small bars: 8x12 horizontal, 12x8 vertical, by
// the two flags of the constructor); the title (the text, or the SysString TitleID) is measured once, then painted in
// grey-blue
INT NCFrameCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCFrameCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bDecorated)
    {
        if (!bVertical)
        {
            if (!bSmall)
            {
                if (BackLeftTex)
                    Device->DrawTexture(0, 0, 16, 20, 0, 0, 16, 20, BackLeftTex, 0xFF, 1);
                if (BackRightTex)
                    Device->DrawTexture((INT)(Width - 16.0f), 0, 16, 20, 0, 0, 16, 20, BackRightTex, 0xFF, 1);
                if (BackMidTex)
                    Device->DrawStretchedTex(16, 0, (INT)(Width - 16.0f - 16.0f), 20, 0, 0, 32, 20, BackMidTex, 0xFF);
                if (TitleX == 0 && TitleY == 0)
                {
                    SIZE Size;
                    const TCHAR* Text;
                    if (Title.Num() == 0 || Title.Num() - 1 == 0)
                        Text = *SysString(TitleID);
                    else
                        Text = *Title;
                    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
                    TitleX = 20;
                    TitleY = (INT)(Height * 0.5f - (Size.cy / 2) + 2.0f);
                }
                if (Title.Num() != 0 && Title.Num() - 1 != 0)
                {
                    Device->DrawText(TitleX, TitleY, 0xFFC8D2DC, *Title, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                }
                else if (TitleID >= 0)
                {
                    Device->DrawText(TitleX, TitleY, 0xFFC8D2DC, *SysString(TitleID), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                                     0, 0, 0);
                }
            }
            else if (bSmall == 1)
            {
                // the stock code tests the first texture and paints the third (and the other way round on the right)
                if (SmallBarTex[0])
                    Device->DrawTexture(0, 0, 8, 12, 0, 0, 8, 12, SmallBarTex[2], 0xFF, 1);
                if (SmallBarTex[1])
                    Device->DrawTexture(8, 0, (INT)(Width - 16.0f), 12, 0, 0, 8, 12, SmallBarTex[1], 0xFF, 1);
                if (SmallBarTex[2])
                    Device->DrawTexture((INT)(Width - 8.0f), 0, 8, 12, 0, 0, 8, 12, SmallBarTex[0], 0xFF, 1);
            }
        }
        else if (bVertical == 1 && bSmall == 1)
        {
            if (SmallBarTex[3])
                Device->DrawTexture(0, 0, 12, 8, 0, 0, 12, 8, SmallBarTex[3], 0xFF, 1);
            if (SmallBarTex[4])
                Device->DrawTexture(0, 8, 12, (INT)(Height - 16.0f), 0, 0, 12, 8, SmallBarTex[4], 0xFF, 1);
            if (SmallBarTex[5])
                Device->DrawTexture(0, (INT)(Height - 8.0f), 12, 8, 0, 0, 12, 8, SmallBarTex[5], 0xFF, 1);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10011c00, NCFrameCtrl, OnPaint)

// 0x10011fe0 (vslot 61) - the move is sent as a delta; LastMouse holds the delta during the call; C3 also tells the
// console which frame is being dragged (NConsoleWnd +0x3C88 in C4 (+0x39E4 in C3), in the tail its header leaves unnamed)
INT NCFrameCtrl::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if (wParam & MK_LBUTTON)
    {
        INT X = (SWORD)LOWORD(lParam);
        INT Y = (SWORD)HIWORD(lParam);
        LastMouseX = X - LastMouseX;
        LastMouseY = Y - LastMouseY;
        if (LastMouseX == 0 && LastMouseY == 0)
            bMoved = 0;
        else
            bMoved = 1;
        SendToParent(WM_MOVE, 0, MAKELPARAM(LastMouseX, LastMouseY));
        LastMouseX = X;
        LastMouseY = Y;
        ::nw::Field<NCFrameCtrl*>(GConsole, 0x3C88) = this;
    }
    return 1;
}
NW_HOOK_METHOD(0x10011fe0, NCFrameCtrl, OnMouseMove)

// 0x10011810 (anonymous in the db, name ours) - the title is a SysString from now on; the text goes
void NCFrameCtrl::SetTitleID(INT ID)
{
    TitleID = ID;
    Title.Empty();
}
NW_HOOK_METHOD(0x10011810, NCFrameCtrl, SetTitleID)

// 0x10011750 (vslot 68) - the parent gets the click too (it comes to the front, takes focus)
INT NCFrameCtrl::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    LockCursor();
    LastMouseX = (SWORD)LOWORD(lParam);
    GrabX = LastMouseX - ScreenX;
    LastMouseY = (SWORD)HIWORD(lParam);
    GrabY = LastMouseY - ScreenY;
    SendToParent(WM_LBUTTONDOWN, wParam, lParam);
    bMoved = 0;
    return 1;
}
NW_HOOK_METHOD(0x10011750, NCFrameCtrl, OnLButtonDown)

// 0x100117b0 (vslot 69)
INT NCFrameCtrl::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    UnlockCursor();
    ReleaseFocus();
    bMoved = 0;
    return 1;
}
NW_HOOK_METHOD(0x100117b0, NCFrameCtrl, OnLButtonUp)

// 0x10011920 (vslot 76) - C3: the stock code still walks the class chain of the parent for four classes (the main,
// inventory, map cover and system menu windows) and does nothing different for them; then the sound, and the
// parent is hidden
INT NCFrameCtrl::OnClose()
{
    NCWnd* Parent = Entry->Parent->Wnd;
    if (!Parent->IsA(NClass::NCMainWnd()))
    {
        if (!Parent->IsA(NClass::NCInventoryWnd()))
        {
            if (!Parent->IsA(NClass::NCMapCoverWnd()))
                Parent->IsA(NClass::NCSystemMenuWnd());
        }
    }
    PlayInterfaceSound(SOUND_WindowClose);
    HideParent();
    return 1;
}
NW_HOOK_METHOD(0x10011920, NCFrameCtrl, OnClose)

// 0x10011650 (vslot 77)
INT NCFrameCtrl::OnMinimize()
{
    Entry->Parent->Wnd->Minimize();
    return 1;
}
NW_HOOK_METHOD(0x10011650, NCFrameCtrl, OnMinimize)

// 0x100117e0 - called by NCWnd::SetTitle; OnPaint measures the title again (TitleX / TitleY back to 0)
void NCFrameCtrl::SetTitleText(const TCHAR* InTitle)
{
    if (InTitle)
    {
        Title = InTitle;
        TitleX = 0;
        TitleY = 0;
    }
}
NW_HOOK_METHOD(0x100117e0, NCFrameCtrl, SetTitleText)

// 0x10011720 (ret 8; anonymous in the db, name ours) - the two shape flags of the constructor set again
void NCFrameCtrl::SetBarShape(INT InVertical, INT InSmall)
{
    bVertical = InVertical;
    bSmall = InSmall;
}
NW_HOOK_METHOD(0x10011720, NCFrameCtrl, SetBarShape)

// 0x10011670 (vslot 69, name ours) - the bar's size, from the window that owns it (NCWnd::SetWindowSize)
void NCFrameCtrl::SetFrameSize(DWORD W, DWORD H)
{
    NCWnd::SetWindowSize((FLOAT)W, (FLOAT)H);
}
NW_HOOK_METHOD(0x10011670, NCFrameCtrl, SetFrameSize)

NW_CHECK_VTABLE(NCFrameCtrl, 0x101a1ab0, 79)

/*-----------------------------------------------------------------------------
    NCResizeFrame.
-----------------------------------------------------------------------------*/

// 0x10012870 - undecorated (the default NCFrameCtrl constructor runs first, then bDecorated is cleared); InternalConstructor
// passes 0
NCResizeFrame::NCResizeFrame(INT InDirection)
{
    ResizeGrabY = 0;
    ResizeGrabX = 0;
    bDecorated = 0;
    Unknown248 = 0;
    bCursorLocked = 0;
    bDragging = 0;
    Direction = InDirection;
}
NW_HOOK_CTOR_ARGS(0x10012870, NCResizeFrame, (INT A), (A))

// 0x100128f0
NW_HOOK_DTOR(0x100128f0, NCResizeFrame)

// 0x100b14c0 (vslot 25) - C2: NCFrameCtrl's alone (no enabled test)
INT NCResizeFrame::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCFrameCtrl::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x100b14c0, NCResizeFrame, WindowProc)

// 0x10011900 (vslot 30)
void NCResizeFrame::KillFocus()
{
    NCWnd::KillFocus();
    bDragging = 0;
    bPressed = 0;
}
NW_HOOK_METHOD(0x10011900, NCResizeFrame, KillFocus)

// 0x10012080 (vslot 67) - resize cursor while over the edge; while dragging, the delta along the resize direction
// goes to the parent as NWM_COMMAND(ID, delta). C3: the delta is measured from where the edge was grabbed
INT NCResizeFrame::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = (SWORD)LOWORD(lParam);
    INT Y = (SWORD)HIWORD(lParam);
    if (PtInRect(X, Y, Rect))
    {
        if (Direction == RESIZE_Vertical)
            SetCursor(5, NULL);
        else if (Direction == RESIZE_Horizontal)
            SetCursor(6, NULL);
        LockCursor();
        bCursorLocked = 1;
        if (!(wParam & MK_LBUTTON) || !bDragging)
            return 1;
    }
    else if (!bDragging || !(wParam & MK_LBUTTON))
    {
        if (bCursorLocked)
        {
            bCursorLocked = 0;
            UnlockCursor();
        }
        return 1;
    }
    LastMouseX = X - ResizeGrabX - ScreenX;
    LastMouseY = Y - ResizeGrabY - ScreenY;
    if (Direction == RESIZE_Vertical)
        NotifyParent(LastMouseY);
    else if (Direction == RESIZE_Horizontal)
        NotifyParent(LastMouseX);
    LastMouseX = X;
    LastMouseY = Y;
    return 1;
}
NW_HOOK_METHOD(0x10012080, NCResizeFrame, OnMouseMove)

// 0x10011830 (vslot 68)
INT NCResizeFrame::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT X = (SWORD)LOWORD(lParam);
    INT Y = (SWORD)HIWORD(lParam);
    LastMouseX = X;
    LastMouseY = Y;
    if (PtInRect(X, Y, Rect))
    {
        ResizeGrabX = LastMouseX - ScreenX;
        ResizeGrabY = LastMouseY - ScreenY;
        bDragging = 1;
        bPressed = 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x10011830, NCResizeFrame, OnLButtonDown)

// 0x100118d0 (vslot 69)
INT NCResizeFrame::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    bPressed = 0;
    bDragging = 0;
    ResizeGrabX = 0;
    ResizeGrabY = 0;
    return 1;
}
NW_HOOK_METHOD(0x100118d0, NCResizeFrame, OnLButtonUp)

NW_CHECK_VTABLE(NCResizeFrame, 0x101a1e30, 79)
