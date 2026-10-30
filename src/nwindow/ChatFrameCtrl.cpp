/*=============================================================================
    ChatFrameCtrl.cpp - the title bar of a conversation window (C3 only; C3 addresses). Stock: WindowProc 0x10119aa0,
    OnPaint 0x1011ab40 .. 0x1011ad8b, the constructors / destructor 0x1011db50 .. 0x1011dbef, OnMinimize 0x101241e0 and
    OnClose 0x10124210 (the three of the WindowProc / OnClose / OnMinimize are L2ChatFrameCtrl's too).
=============================================================================*/
#include "nwindow/ChatFrameCtrl.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

// 0x101426c0 - the base constructor, then the vtable
ChatFrameCtrl::ChatFrameCtrl()
{
}
NW_HOOK_CTOR(0x101426c0, ChatFrameCtrl)

// 0x10142760 (ret 0xC) - the frame's three arguments
ChatFrameCtrl::ChatFrameCtrl(INT InDecorated, INT InVertical, INT InSmall)
    : NCFrameCtrl(InDecorated, InVertical, InSmall)
{
}
NW_HOOK_CTOR_ARGS(0x10142760, ChatFrameCtrl, (INT A, INT B, INT C), (A, B, C))

// 0x10142710
NW_HOOK_DTOR(0x10142710, ChatFrameCtrl)

// 0x1013e340 (vslot 25) - the frame's, then its two buttons
INT ChatFrameCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCFrameCtrl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_FRAME_MINIMIZE)
            return OnMinimize() != 0;
        if (wParam == NID_FRAME_CLOSE)
            return OnClose() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1013e340, ChatFrameCtrl, WindowProc)

// 0x10148d10 (vslot 77) - the window this bar belongs to hears the minimize button
INT ChatFrameCtrl::OnMinimize()
{
    NWndEntry* Parent = Entry->Parent;
    if (Parent && Parent->Wnd)
    {
        Parent->Wnd->WindowProc(NWM_COMMAND, NID_FRAME_MINIMIZE, 0);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x10148d10, ChatFrameCtrl, OnMinimize)

// 0x10148d40 (vslot 76) - ... and the close button
INT ChatFrameCtrl::OnClose()
{
    NWndEntry* Parent = Entry->Parent;
    if (Parent && Parent->Wnd)
    {
        Parent->Wnd->WindowProc(NWM_COMMAND, NID_FRAME_CLOSE, 0);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x10148d40, ChatFrameCtrl, OnClose)

// 0x1013f430 (vslot 62) - the three pieces of the bar (16 | stretched | 16, 20 high), then the title: the first paint measures it and puts
// it at x = 20, vertically centered; the text is "<title> - <SysString 1002>" cut to the width less 56. C4: the title is the SysString of
// TitleID (NCFrameCtrl +0x200; the C3 Title string is not read here); without one (TitleID < 0) the text starts with nothing
INT ChatFrameCtrl::OnPaint(NCGDevice* Device)
{
    TCHAR Line[2048];
    TCHAR Cut[2048];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackLeftTex)
        Device->DrawTexture(0, 0, 0x10, 0x14, 0, 0, 0x10, 0x14, BackLeftTex, 0xFF, 1);
    if (BackRightTex)
        Device->DrawTexture((INT)(Width - 16.0f), 0, 0x10, 0x14, 0, 0, 0x10, 0x14, BackRightTex, 0xFF, 1);
    if (BackMidTex)
        Device->DrawStretchedTex(0x10, 0, (INT)(Width - 16.0f - 16.0f), 0x14, 0, 0, 0x20, 0x14, BackMidTex, 0xFF);
    if (TitleX == 0 && TitleY == 0 && TitleID >= 0)
    {
        NCGDevice::GetTextExtent(&Size, *GL2GameData.SysString[TitleID], 0, 0, 0, 0, 0, (L2FontType)0);
        TitleX = 0x14;
        TitleY = (INT)(Height * 0.5f - (Size.cy / 2) + 2.0f);
    }
    if (TitleID >= 0)
        appSprintf(Line, TEXT("%s - %s"), *GL2GameData.SysString[TitleID], *GL2GameData.SysString[1002]);
    else
        appSprintf(Line, TEXT("%s - %s"), TEXT(""), *GL2GameData.SysString[1002]);
    NCGDevice::GetTextExtent(&Size, Line, 0, 0, 0, 0, 0, (L2FontType)0);
    NFitText(Line, Cut, (INT)(Width - 16.0f - 40.0f), 0xC);
    Device->DrawText(TitleX, TitleY, 0xFFDCDCDC, Cut, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x1013f430, ChatFrameCtrl, OnPaint)

NW_CHECK_VTABLE(ChatFrameCtrl, 0x101d98d8, 79)
