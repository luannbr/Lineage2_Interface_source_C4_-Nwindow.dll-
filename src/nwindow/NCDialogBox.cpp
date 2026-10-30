/*=============================================================================
    NCDialogBox.cpp - the 0x1A4-byte base of the warning boxes. Stock: 0x10006d10 .. 0x10006e1f
    (the two modal helpers sit between its own functions), 0x10006f10 .. 0x10006f83, 0x100090a0.
    The message box of C1 (OK / Cancel, the edit line, the keypad) is NCGaraDialogBox in C2
    (NCGaraDialogBox.cpp, 0x10006f90 and the code around it).
=============================================================================*/
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

/*-----------------------------------------------------------------------------
    C2: NCDialogBox (CDialogBox, 0x1A4) is a thin base of the warning boxes (NCWarningDialogBox,
    NCKeyCryptWarningDialogBox) and has no state of its own: the message box of C1 went to
    NCGaraDialogBox (0x23C, constructor 0x10006f90; OnCreate 0x10007110, OnPaint 0x10007560,
    OnDestroy 0x100076f0, OnOK 0x10007b20 ...). The non-virtual functions of C1 that the function
    map paired with NCGaraDialogBox's code (Reset, SetButtonTexts, GetOwner, UpdateLayout,
    SetInfo) keep a forwarding definition here: the other windows reach the console's boxes
    through an NCDialogBox*.
-----------------------------------------------------------------------------*/

// 0x1000b350 - the stock constructor is NCWnd's with NControl's fields inline (ID 0, no disable texture, NotifyID -1,
// an empty caption)
NCDialogBox::NCDialogBox()
{
}
NW_HOOK_CTOR(0x1000b350, NCDialogBox)


// 0x10090b60 (vslot 25) - NControl's, as a boolean; the stock body is one with NCButton's (hooked there)
INT NCDialogBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10008340 (vslot 37) - NControl's OnCreate and 1 (one body for several classes)
INT NCDialogBox::OnCreate()
{
    NControl::OnCreate();
    return 1;
}
NW_HOOK_METHOD(0x10008340, NCDialogBox, OnCreate)

// 0x10008350 (vslot 38) - guarded
INT NCDialogBox::OnDestroy()
{
    guard(NCDialogBox::OnDestroy);
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10008350, NCDialogBox, OnDestroy)

// 0x10008480 (vslot 49) - a modal box stays
void NCDialogBox::HideWindow()
{
    if (!(GetStyle() & NWS_MODAL))
        NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x10008480, NCDialogBox, HideWindow)

// 0x100083f0 - guarded: shown (NCWnd's, the override would stay hidden while modal), made modal and focused
void NCDialogBox::DoModal()
{
    guard(NCDialogBox::DoModal);
    NCWnd::ShowWindow();
    SetModal(Entry);
    SetFocus();
    unguard;
}
NW_HOOK_METHOD(0x100083f0, NCDialogBox, DoModal)

// 0x100083d0 - and goes away (NCWnd's HideWindow: the override would stay while modal)
void NCDialogBox::HideModal()
{
    NCWnd::HideWindow();
    ClearModal(Entry);
}
NW_HOOK_METHOD(0x100083d0, NCDialogBox, HideModal)

/*-----------------------------------------------------------------------------
    The five functions the other windows call through an NCDialogBox*. The object behind that pointer
    (NCConsole::MainDialogBox / SecondDialogBox, NConsoleWnd::DialogBox) is an NCGaraDialogBox in C2: these
    are its functions (NCGaraDialogBox.cpp hooks the stock bodies), so each one hands the call on to the
    real class. Only a pointer to a message box may call them (the warning boxes derived from this class
    have no such state). Code that can name the class holds an NCGaraDialogBox* instead.
-----------------------------------------------------------------------------*/

// 0x10006740 - NCGaraDialogBox::Reset
void NCDialogBox::Reset()
{
    ((NCGaraDialogBox*)this)->Reset();
}

// 0x10006c50 - NCGaraDialogBox::SetButtonTexts
void NCDialogBox::SetButtonTexts(INT OKTextID, INT CancelTextID)
{
    ((NCGaraDialogBox*)this)->SetButtonTexts(OKTextID, CancelTextID);
}

// 0x10006cf0 - NCGaraDialogBox::GetOwner
NWndEntry* NCDialogBox::GetOwner()
{
    return ((NCGaraDialogBox*)this)->GetOwner();
}

// 0x10007860 - NCGaraDialogBox::UpdateLayout
void NCDialogBox::UpdateLayout()
{
    ((NCGaraDialogBox*)this)->UpdateLayout();
}

// 0x10007a00 - NCGaraDialogBox::SetInfo
void NCDialogBox::SetInfo(NDialogInfo* Info)
{
    ((NCGaraDialogBox*)this)->SetInfo(Info);
}

// 0x1000b120 - the vtable, ConditionalDestroy, NControl
NW_HOOK_DTOR(0x1000b120, NCDialogBox)

NW_CHECK_VTABLE(NCDialogBox, 0x101a1070, 76)
