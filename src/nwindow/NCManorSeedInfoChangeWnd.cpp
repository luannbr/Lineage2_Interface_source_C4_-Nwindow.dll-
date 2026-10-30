/*=============================================================================
    NCManorSeedInfoChangeWnd.cpp - the box that sets one seed's amount and price in the lord's table (new in C4): the twin
    of NCManorCropInfoChangeWnd without the price kind.
    Stock (C4): 0x1005F880 (constructor), 0x1005F900 (destructor), 0x10137D70 .. 0x10138730 (HideWindow, DoModal, the
    setters, OnOKBtnClick, OnCancelBtnClick, WindowProc, OnDestroy, OnPaint, OnCreate).
=============================================================================*/
#include "nwindow/NCManorSeedInfoChangeWnd.h"
#include "nwindow/NCManorSeedInfoSettingWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels and system messages (names ours); the labels of the fields are SysString[1139], [1224] and [1140]
// (GL2GameData + 0x9871C + 12 n: 0x9BC80, 0x9C07C, 0x9BC8C)
enum
{
    STR_OK = 140,
    STR_Cancel = 141,
    MSG_BadAmount = 0x616,
    MSG_BadPrice = 0x615,
};

// 0x1005f880 - NCDialogBox's constructor, the vtable and the empty seed name
NCManorSeedInfoChangeWnd::NCManorSeedInfoChangeWnd()
{
}
NW_HOOK_CTOR(0x1005f880, NCManorSeedInfoChangeWnd)

// 0x1005f900 - ConditionalDestroy, the seed name, NCDialogBox's destructor
NW_HOOK_DTOR(0x1005f900, NCManorSeedInfoChangeWnd)

// 0x10138390 (vslot 25) - the base's answer first; then OK and Cancel
INT NCManorSeedInfoChangeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSEEDCHANGE_OK)
            return OnOKBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSEEDCHANGE_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10138390, NCManorSeedInfoChangeWnd, WindowProc)

// 0x10137d70 (vslot 49) - the modal window goes (HideWindow, then the modal state is cleared)
void NCManorSeedInfoChangeWnd::HideWindow()
{
    guard(NCManorSeedInfoChangeWnd::HideWindow);
    NCDialogBox::HideModal();
    unguard;
}
NW_HOOK_METHOD(0x10137d70, NCManorSeedInfoChangeWnd, HideWindow)

// 0x10137de0 (vslot 75) - the amount edit takes the focus, then the window comes up modal (the stock guard names the crop
// box: the source was copied)
void NCManorSeedInfoChangeWnd::NewVirtual75()
{
    guard(NCManorCropInfoChangeWnd::DoModal);
    AmountEdit->SetFocus();
    NCDialogBox::DoModal();
    unguard;
}
NW_HOOK_METHOD(0x10137de0, NCManorSeedInfoChangeWnd, NewVirtual75)

// 0x10137e60 (ret 0xc)
void NCManorSeedInfoChangeWnd::SetSeedName(FString Name)
{
    guard(NCManorSeedInfoChangeWnd::SetSeedName);
    SeedName = Name;
    unguard;
}
NW_HOOK_METHOD(0x10137e60, NCManorSeedInfoChangeWnd, SetSeedName)

// 0x10137ef0 (ret 0xc)
void NCManorSeedInfoChangeWnd::SetAmt(FString Text)
{
    guard(NCManorSeedInfoChangeWnd::SetAmt);
    AmountEdit->Left = Text;
    unguard;
}
NW_HOOK_METHOD(0x10137ef0, NCManorSeedInfoChangeWnd, SetAmt)

// 0x10137f90 (ret 0xc)
void NCManorSeedInfoChangeWnd::SetPrice(FString Text)
{
    guard(NCManorSeedInfoChangeWnd::SetPrice);
    PriceEdit->Left = Text;
    unguard;
}
NW_HOOK_METHOD(0x10137f90, NCManorSeedInfoChangeWnd, SetPrice)

// 0x10138030 (ret 8; anonymous in the db) - the lowest and the highest price OK accepts
void NCManorSeedInfoChangeWnd::SetLimits(INT InMinPrice, INT InMaxPrice)
{
    MinPrice = InMinPrice;
    MaxPrice = InMaxPrice;
}
NW_HOOK_METHOD(0x10138030, NCManorSeedInfoChangeWnd, SetLimits)

// 0x10138310 - closes the window
INT NCManorSeedInfoChangeWnd::OnCancelBtnClick()
{
    guard(NCManorSeedInfoChangeWnd::OnCancelBtnClick);
    NCDialogBox::HideModal();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138310, NCManorSeedInfoChangeWnd, OnCancelBtnClick)

// 0x10138400 (vslot 38) - the two buttons and the two edit boxes, then the base's
INT NCManorSeedInfoChangeWnd::OnDestroy()
{
    guard(NCManorSeedInfoChangeWnd::OnDestroy);
    ManorDestroyChild(this, OKButton);
    ManorDestroyChild(this, CancelButton);
    ManorDestroyChild(this, AmountEdit);
    ManorDestroyChild(this, PriceEdit);
    NCDialogBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138400, NCManorSeedInfoChangeWnd, OnDestroy)

// 0x10138530 (vslot 62) - the back texture under the title bar, the seed's name, the three labels of the fields and the
// most the table has
INT NCManorSeedInfoChangeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorSeedInfoChangeWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    ManorDrawLabel(Device, 0x15, 0x28, 0xFFDCDCDC, *SeedName);
    ManorDrawLabel(Device, 0x1F, 0x47, 0xFFDCDCDC, *SysString(1139));
    ManorDrawLabel(Device, 0x1F, 0x60, 0xFFDCDCDC, *SysString(1224));
    ManorDrawLabel(Device, 0x1F, 0x79, 0xFFDCDCDC, *SysString(1140));
    FString Max = ManorMakeNumStr(MaxAmount);
    ManorDrawLabel(Device, 0x91, 0x60, 0xFFDCDCDC, *Max);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138530, NCManorSeedInfoChangeWnd, OnPaint)

// 0x10138050 - OK: the amount must be between 0 and the most the table has, the price (when there is an amount) between the
// two limits; then the box closes, the two numbers (amount, price) go to the table's window (the desktop's, +0x1F0), which
// takes the focus. Otherwise the player is told the allowed range (system messages 0x615 / 0x616)
INT NCManorSeedInfoChangeWnd::OnOKBtnClick()
{
    guard(NCManorSeedInfoChangeWnd::OnOKBtnClick);
    TCHAR Message[1024];
    INT Amount = appAtoi(*AmountEdit->Left);
    if (Amount >= 0 && MaxAmount >= Amount)
    {
        INT Price = appAtoi(*PriceEdit->Left);
        if (Amount == 0 || (MinPrice <= Price && MaxPrice >= Price))
        {
            NCDialogBox::HideModal();
            NCManorSeedInfoSettingWnd* Setting = (NCManorSeedInfoSettingWnd*)ManorDesktopWnd(0x1F0);
            Setting->NotifyChangeTomValue(Amount, Price);
            ManorDesktopWnd(0x1F0)->SetFocus();
            return 1;
        }
        L2ParamStack Low(10);
        Low.PushBack((void*)1);
        Low.PushBack((void*)(INT_PTR)MinPrice);
        GConsole->AddSystemMessageParam(Low);
        L2ParamStack High(10);
        High.PushBack((void*)1);
        High.PushBack((void*)(INT_PTR)MaxPrice);
        GConsole->AddSystemMessageParam(High);
        L2ParamStack Text(10);
        Text.PushBack((void*)MSG_BadPrice);
        ::nw::VirtualCall<void>(GConsole, 191, &Text, 1, Message, 0);
        ::nw::VirtualCall<void>(GConsole, 28, Message, 0);
        return 1;
    }
    L2ParamStack Low(10);
    Low.PushBack((void*)1);
    Low.PushBack((void*)0);
    GConsole->AddSystemMessageParam(Low);
    L2ParamStack High(10);
    High.PushBack((void*)1);
    High.PushBack((void*)(INT_PTR)MaxAmount);
    GConsole->AddSystemMessageParam(High);
    L2ParamStack Text(10);
    Text.PushBack((void*)MSG_BadAmount);
    ::nw::VirtualCall<void>(GConsole, 191, &Text, 1, Message, 0);
    ::nw::VirtualCall<void>(GConsole, 28, Message, 0);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138050, NCManorSeedInfoChangeWnd, OnOKBtnClick)

// 0x10138730 (vslot 37) - the base's OnCreate, the back texture, the title bar, OK and Cancel, the two edit boxes (ten
// characters)
INT NCManorSeedInfoChangeWnd::OnCreate()
{
    guard(NCManorSeedInfoChangeWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor4_back"));

    FrameCtrl = ManorNewFrameCtrl(1, 0, 0);
    FrameID = (INT)0xFFDCCC20;
    FrameCtrl->ID = (INT)0xFFDCCC20;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    OKButton = ManorNewButton(0, 0);
    OKButton->ID = NID_MANORSEEDCHANGE_OK;
    OKButton->SetPosition(0x33, 0xAC, 0, 0);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORSEEDCHANGE_CANCEL;
    CancelButton->SetPosition(0x83, 0xAC, 0, 0);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    AmountEdit = new NCEditBox(0, NULL, 1, 0x108, 0);
    AmountEdit->ID = (INT)0xFFDCCC10;
    AmountEdit->SetPosition(0x91, 0x44, 0x44, 0x11);
    CreateChild(AmountEdit, NWS_VISIBLE);
    AmountEdit->SetMaxLength(10);

    PriceEdit = new NCEditBox(0, NULL, 1, 0x108, 0);
    PriceEdit->ID = (INT)0xFFDCCC11;
    PriceEdit->SetPosition(0x91, 0x76, 0x44, 0x11);
    CreateChild(PriceEdit, NWS_VISIBLE);
    PriceEdit->SetMaxLength(10);

    SeedName = TEXT("");
    MaxAmount = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138730, NCManorSeedInfoChangeWnd, OnCreate)

NW_CHECK_VTABLE(NCManorSeedInfoChangeWnd, 0x101ae3e8, 76)
