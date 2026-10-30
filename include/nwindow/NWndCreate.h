#pragma once
// The children of a C4 window, made the way its stock OnCreate makes them (helpers of the status2 lane: the status
// panels, the party window, the clan page, the pet and summon windows, the skill and train windows).
//
// The stock code allocates the object with UObject::StaticAllocateObject (the class, the transient package, no name),
// tests the result and runs the stock constructor on that memory. That is what the class's `new` expression does (its
// operator new is StaticAllocateObject of the class in the transient package, then the constructor): the constructors of
// these classes are real code of ours (NW_HOOK_CTOR_ARGS over the real body), so the helpers are plain `new`s (stage 5:
// no call into the stock image). Each takes the stock constructor's own arguments.

#include "nwindow/NCWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCProgressCtrl.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NCNameCtrl.h"
#include "nwindow/NCGDevice.h"

static FORCEINLINE NCWnd* AllocChild(UClass* Class)
{
    return (NCWnd*)UObject::StaticAllocateObject(Class, (UObject*)UObject::GetTransientPackage(), NAME_None, 0);
}

// NCButton(Normal, Pushed, bAcceptKey): 0x10003340
static NCButton* NewButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey)
{
    return new NCButton(NormalTexName, PushedTexName, bInAcceptKey);
}

// NCFrameCtrl(Decorated, Vertical, Small): 0x100126E0 (a title bar; (0, 0, 0) is the undecorated one)
static NCFrameCtrl* NewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// NCResizeFrame(Direction): 0x10012870 (the right edge of a status window, which widens it)
static NCResizeFrame* NewResizeFrame(INT Direction)
{
    return new NCResizeFrame(Direction);
}

// NCNameCtrl() (0x1002D260, class data at 0x102C4EE8): the label of a window's name
static NCWnd* NewNameCtrl()
{
    return new NCNameCtrl;
}

// NCProgressCtrl(Back, Bar, Border): 0x10007830
static NCProgressCtrl* NewProgressCtrl(const TCHAR* BackTexName, const TCHAR* BarTexName, INT Border)
{
    return new NCProgressCtrl(BackTexName, BarTexName, Border);
}

// a window of one of the lane's own classes: allocated like the others, then its default constructor (CtorVA, the stock one;
// the standalone runs the class's own constructor, which the compiler picks by the class)
template <class T> static T* NewWindowOf(UINT CtorVA)
{
#if NW_HOOK_MODE
    T* Wnd = (T*)AllocChild(T::StaticClass());
    if (Wnd)
        ::nw::StockCallAt<void>(CtorVA, Wnd);
    return Wnd;
#else
    (void)CtorVA;
    return new T;
#endif
}

// NCTabControl(): 0x1002E750
static NCTabControl* NewTabControl()
{
    return new NCTabControl;
}

// NCTabControl::AddTab(TextID, Page, W, H, NormalTex, PushedTex, Gap, ToolTipID, bDragable): 0x1002DED0 (ret 0x24: this client
// has one more argument than C3's, which NCTabControl.h declares now; the last tab's is 0 in the pet and summon windows)
static FORCEINLINE INT AddTabC4(NCTabControl* Tabs, INT TextID, NCWnd* Page, INT W, INT H, const TCHAR* NormalTexName,
                                const TCHAR* PushedTexName, INT Gap, INT ToolTipID, INT Unknown)
{
    return Tabs->AddTab(TextID, Page, W, H, NormalTexName, PushedTexName, Gap, ToolTipID, Unknown);
}

// the name control's byte at +0x2A0 that the stock OnCreates clear (the paints set the text)
static FORCEINLINE void ClearNameCtrlFlag(NCWnd* NameCtrl)
{
    ::nw::Field<BYTE>(NameCtrl, 0x2A0) = 0;
}

// sub_1002c760 (ret 0x10, on the name control; defined in NCStatusWnd.cpp as NCNameCtrlView::SetText): the text of a window's
// name label and its color; the two flags are (0, 1) in the target window's paint and (0, 0) in the player's
void NameCtrlSetText(NCWnd* NameCtrl, const TCHAR* Text, INT FlagA, INT FlagB, DWORD Color);
static FORCEINLINE void SetNameText(NCWnd* NameCtrl, const TCHAR* Text, INT FlagA, INT FlagB, DWORD Color)
{
    NameCtrlSetText(NameCtrl, Text, FlagA, FlagB, Color);
}

// NCGDevice::DrawSpecialDigit(bLarge, X, Y, Color, Text, Alpha): 0x10013350 (ret 0x18: this client has the alpha; the stock
// callers pass 0xFF)
static FORCEINLINE void DrawDigit(NCGDevice* Device, UBOOL bLarge, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawSpecialDigit(bLarge, X, Y, Color, (TCHAR*)Text, 0xFF);
}

// UObject::IsA, always inline (the stock code spells the walk of the class chain out at each test; the compiler may turn
// the header's inline into one shared call when a function tests twice)
static __forceinline UBOOL IsWindowOf(NCWnd* Wnd, UClass* Class)
{
    for (UField* Link = reinterpret_cast<UField*>(Wnd->GetClass()); Link; Link = Link->SuperField)
        if (Link == reinterpret_cast<UField*>(Class))
            return 1;
    return 0;
}

// one child of an OnDestroy (repeated inline in the stock code): removed from the window, deleted, cleared
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}
