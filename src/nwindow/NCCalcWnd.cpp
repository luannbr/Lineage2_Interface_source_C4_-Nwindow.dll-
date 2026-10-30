/*=============================================================================
    NCCalcWnd.cpp - the calculator window (new in C2). Stock (C3): 0x10109c00 .. 0x10109c9f (WindowProc,
    SetFocus), 0x1010a0d0 .. 0x1010a4cf, 0x1010a9c0 .. 0x1010a9ff and 0x1010ac10 .. 0x1010afe2.
=============================================================================*/
#include "nwindow/NCCalcWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCalcEditCtrl.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"

// ids (names ours)
enum
{
    NID_CALC_CLOSE = (INT)0xFF01FF00,
    NID_CALC_KEY = (INT)0xFF01FF01,      // NotifyID = the key
    NID_CALC_DISPLAY = (INT)0xFF01FF02,
};
enum { SYSSTR_CalcClose = 646 };         // the close button's text (name ours)

#if NW_HOOK_MODE
// the textures of the keys: pairs of names (up, down), 19 of them, in the stock image
#define GCalcKeyTextures (reinterpret_cast<const TCHAR* const (*)[2]>(::nw::Addr(0x1027D278)))
#else
extern const TCHAR* const GCalcKeyTextures[][2];
#endif

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x1011faf0 - NCWnd's constructor and the vtable; the fields are left as allocated
NCCalcWnd::NCCalcWnd()
{
}
NW_HOOK_CTOR(0x1011faf0, NCCalcWnd)

// 0x1011fb40
NW_HOOK_DTOR(0x1011fb40, NCCalcWnd)


// 0x1011ec30 (vslot 25)
INT NCCalcWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_CALC_CLOSE)
            return OnClose() != 0;
        if ((INT)wParam == NID_CALC_KEY)
            return OnKey(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1011ec30, NCCalcWnd, WindowProc)

// 0x1011ecb0 (vslot 29) - the display takes the focus
void NCCalcWnd::SetFocus()
{
    Display->SetFocus();
}
NW_HOOK_METHOD(0x1011ecb0, NCCalcWnd, SetFocus)

// 0x1011f420 - the display's Reset
void NCCalcWnd::Reset()
{
    Display->Reset();
}
NW_HOOK_METHOD(0x1011f420, NCCalcWnd, Reset)

// 0x1011f140 (vslot 74) - a key of the keypad: the character it stands for goes to the display
// (WM_CHAR), the backspace as WM_KEYDOWN, "00" as two zeros; the two clears act on the display
// directly. The display has the focus afterwards.
INT NCCalcWnd::OnKey(INT Key)
{
    switch (Key)
    {
    case 0:
        Display->SendTo(Display->Entry, WM_CHAR, '7', 0);
        break;
    case 1:
        Display->SendTo(Display->Entry, WM_CHAR, '8', 0);
        break;
    case 2:
        Display->SendTo(Display->Entry, WM_CHAR, '9', 0);
        break;
    case 3:
        Display->SendTo(Display->Entry, WM_CHAR, '+', 0);
        break;
    case 4:
        Display->ShowZero();
        Display->SetFocus();
        return 1;
    case 5:
        Display->SendTo(Display->Entry, WM_CHAR, '4', 0);
        break;
    case 6:
        Display->SendTo(Display->Entry, WM_CHAR, '5', 0);
        break;
    case 7:
        Display->SendTo(Display->Entry, WM_CHAR, '6', 0);
        break;
    case 8:
        Display->SendTo(Display->Entry, WM_CHAR, '-', 0);
        break;
    case 9:
        Display->Reset();
        Display->SetFocus();
        return 1;
    case 10:
        Display->SendTo(Display->Entry, WM_CHAR, '1', 0);
        break;
    case 11:
        Display->SendTo(Display->Entry, WM_CHAR, '2', 0);
        break;
    case 12:
        Display->SendTo(Display->Entry, WM_CHAR, '3', 0);
        break;
    case 13:
        Display->SendTo(Display->Entry, WM_CHAR, '*', 0);
        break;
    case 14:
        Display->SendTo(Display->Entry, WM_KEYDOWN, VK_BACK, 0);
        break;
    case 16:
        Display->SendTo(Display->Entry, WM_CHAR, '0', 0);
        // falls through: the second zero
    case 15:
        Display->SendTo(Display->Entry, WM_CHAR, '0', 0);
        break;
    case 17:
        Display->SendTo(Display->Entry, WM_CHAR, '/', 0);
        break;
    case 18:
        Display->SendTo(Display->Entry, WM_CHAR, '=', 0);
        break;
    }
    Display->SetFocus();
    return 1;
}
NW_HOOK_METHOD(0x1011f140, NCCalcWnd, OnKey)

// 0x1011f950 (vslot 75) - closing empties the calculator
INT NCCalcWnd::OnClose()
{
    Reset();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x1011f950, NCCalcWnd, OnClose)

// 0x1011f970 (vslot 48) - it opens empty
void NCCalcWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    Reset();
}
NW_HOOK_METHOD(0x1011f970, NCCalcWnd, ShowWindow)

// 0x1011fb90 (vslot 37) - the title bar, the display and the keypad in rows of five (the "00" key is wider and the last row
// has four keys, 4 pixels apart), then the close button under them. C3: the CH3 textures, the keys 35 pixels wide.
INT NCCalcWnd::OnCreate()
{
    INT KeyW = 35;
    INT X;
    BackTex = FindTexture(TEXT("l2ui_ch3.calculate.calculate2_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    Display = new NCCalcEditCtrl();
    Display->ID = NID_CALC_DISPLAY;
    Display->SetPosition(34, 40, 188, 17);
    CreateChild(Display, NWS_VISIBLE);
    Display->Unknown1CC = 0;   // NCEditBox: the caret flag (stock: +0x21C)

    INT Y = 66;
    for (INT i = 0; i < 19; i++)
    {
        if (i == 0 || i == 5 || i == 10 || i == 15)
        {
            X = 35;
            if (i != 0)
                Y += 27;
        }
        else
            X = X + 4 + KeyW;
        KeyW = (INT)(((i != 16) - 1) & 0x26) + 35;
        Keys[i] = new NCButton(GCalcKeyTextures[i][0], GCalcKeyTextures[i][1], 0);
        Keys[i]->ID = NID_CALC_KEY;
        Keys[i]->NotifyID = i;
        Keys[i]->SetPosition(X, Y, KeyW, 24);
        CreateChild(Keys[i], NWS_VISIBLE);
    }

    CloseButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CloseButton->ID = NID_CALC_CLOSE;
    CloseButton->SetPosition(91, 182, 76, 23);
    CreateChild(CloseButton, NWS_VISIBLE);
    CloseButton->SetTextID(SYSSTR_CalcClose);
    return 1;
}
NW_HOOK_METHOD(0x1011fb90, NCCalcWnd, OnCreate)

// 0x1011f030 (vslot 38) - guarded; the display, the keys and the close button go, then NCWnd's
INT NCCalcWnd::OnDestroy()
{
    guard(NCCalcWnd::OnDestroy);
    DestroyChild(this, Display);
    for (INT i = 0; i < 19; i++)
        DestroyChild(this, Keys[i]);
    DestroyChild(this, CloseButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011f030, NCCalcWnd, OnDestroy)

// 0x1011f310 (vslot 56) - the panel under the title bar (the window's size less the bar's 20 pixels); the texture is looked
// up again at each paint; guarded
INT NCCalcWnd::OnPaint(NCGDevice* Device)
{
    guard(NCCalcWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    BackTex = FindTexture(TEXT("l2ui_ch3.calculate.calculate2_back"));
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011f310, NCCalcWnd, OnPaint)

NW_CHECK_VTABLE(NCCalcWnd, 0x101d08f0, 76)
