/*=============================================================================
    NCManorCropInfoChangeWnd.cpp - the box that sets one crop's amount and price in the lord's table (new in C4).
    Stock (C4): 0x1005F970 (constructor), 0x1012F780 .. 0x10130260 (HideWindow, DoModal, the setters, the handlers,
    OnDestroy, OnPaint, OnOKBtnClick, WindowProc), 0x10130260 (OnCreate).
=============================================================================*/
#include "nwindow/NCManorCropInfoChangeWnd.h"
#include "nwindow/NCManorCropInfoSettingWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels and system messages (names ours)
enum
{
    STR_OK = 140,
    STR_Cancel = 141,
    MSG_BadAmount = 0x618,
    MSG_BadPrice = 0x617,
};

// 0x1005f970 - NCDialogBox's constructor, the vtable and the empty crop name
NCManorCropInfoChangeWnd::NCManorCropInfoChangeWnd()
{
}
NW_HOOK_CTOR(0x1005f970, NCManorCropInfoChangeWnd)

// 0x1005f9f0 - ConditionalDestroy, the crop name, NCDialogBox's destructor
NW_HOOK_DTOR(0x1005f9f0, NCManorCropInfoChangeWnd)

// 0x101301f0 (vslot 25) - the base's answer first; then OK and Cancel
INT NCManorCropInfoChangeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORCHANGE_OK)
            return OnOKBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORCHANGE_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x101301f0, NCManorCropInfoChangeWnd, WindowProc)

// 0x1012f780 (vslot 49) - the modal window goes (HideWindow, then the modal state is cleared)
void NCManorCropInfoChangeWnd::HideWindow()
{
    guard(NCManorCropInfoChangeWnd::HideWindow);
    NCDialogBox::HideModal();
    unguard;
}
NW_HOOK_METHOD(0x1012f780, NCManorCropInfoChangeWnd, HideWindow)

// 0x1012f7f0 (vslot 75) - the amount edit takes the focus, then the window comes up modal
void NCManorCropInfoChangeWnd::NewVirtual75()
{
    guard(NCManorCropInfoChangeWnd::DoModal);
    AmountEdit->SetFocus();
    NCDialogBox::DoModal();
    unguard;
}
NW_HOOK_METHOD(0x1012f7f0, NCManorCropInfoChangeWnd, NewVirtual75)

// 0x1012f870 (ret 0xc)
void NCManorCropInfoChangeWnd::SetCropName(FString Name)
{
    guard(NCManorCropInfoChangeWnd::SetCropName);
    CropName = Name;
    unguard;
}
NW_HOOK_METHOD(0x1012f870, NCManorCropInfoChangeWnd, SetCropName)

// 0x1012f900 (ret 4; anonymous in the db)
void NCManorCropInfoChangeWnd::SetMaxAmount(INT Value)
{
    MaxAmount = Value;
}
NW_HOOK_METHOD(0x1012f900, NCManorCropInfoChangeWnd, SetMaxAmount)

// 0x1012f910 (ret 0xc)
void NCManorCropInfoChangeWnd::SetAmt(FString Text)
{
    guard(NCManorCropInfoChangeWnd::SetAmt);
    AmountEdit->Left = Text;
    unguard;
}
NW_HOOK_METHOD(0x1012f910, NCManorCropInfoChangeWnd, SetAmt)

// 0x1012f9b0 (ret 0xc)
void NCManorCropInfoChangeWnd::SetPriceText(FString Text)
{
    guard(NCManorCropInfoChangeWnd::SetPrice);
    PriceEdit->Left = Text;
    unguard;
}
NW_HOOK_METHOD(0x1012f9b0, NCManorCropInfoChangeWnd, SetPriceText)

// 0x1012fa50 (ret 4) - the kind of the price (1 .. 2, 0 counts as 1) selects its item of the combo box
void NCManorCropInfoChangeWnd::SetPriceKind(INT Kind)
{
    guard(NCManorCropInfoChangeWnd::SetPrice);
    if (Kind == 0)
        Kind = 1;
    PriceCombo->SetSelected(Kind - 1);
    unguard;
}
NW_HOOK_METHOD(0x1012fa50, NCManorCropInfoChangeWnd, SetPriceKind)

// 0x1012fae0 (ret 8; anonymous in the db) - the lowest and the highest price OK accepts
void NCManorCropInfoChangeWnd::SetLimits(INT InMinPrice, INT InMaxPrice)
{
    MinPrice = InMinPrice;
    MaxPrice = InMaxPrice;
}
NW_HOOK_METHOD(0x1012fae0, NCManorCropInfoChangeWnd, SetLimits)

// 0x1012fb00 - closes the window
INT NCManorCropInfoChangeWnd::OnCancelBtnClick()
{
    guard(NCManorCropInfoChangeWnd::OnCancelBtnClick);
    NCDialogBox::HideModal();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012fb00, NCManorCropInfoChangeWnd, OnCancelBtnClick)

// 0x1012fb80 (vslot 38) - the two buttons, the two edit boxes and the combo box, then the base's
INT NCManorCropInfoChangeWnd::OnDestroy()
{
    guard(NCManorCropInfoChangeWnd::OnDestroy);
    ManorDestroyChild(this, OKButton);
    ManorDestroyChild(this, CancelButton);
    ManorDestroyChild(this, AmountEdit);
    ManorDestroyChild(this, PriceEdit);
    ManorDestroyChild(this, PriceCombo);
    NCDialogBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012fb80, NCManorCropInfoChangeWnd, OnDestroy)

// 0x1012fcd0 (vslot 62) - the back texture under the title bar, the crop's name, the four labels of the fields and the
// most the table has
INT NCManorCropInfoChangeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorCropInfoChangeWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    ManorDrawLabel(Device, 0x15, 0x28, 0xFFDCDCDC, *CropName);
    ManorDrawLabel(Device, 0x1F, 0x47, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9C088));
    ManorDrawLabel(Device, 0x1F, 0x60, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9C094));
    ManorDrawLabel(Device, 0x1F, 0x79, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BCF8));
    ManorDrawLabel(Device, 0x1F, 0x92, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BD04));
    FString Max = ManorMakeNumStr(MaxAmount);
    ManorDrawLabel(Device, 0x91, 0x60, 0xFFDCDCDC, *Max);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012fcd0, NCManorCropInfoChangeWnd, OnPaint)

// 0x1012ff00 - OK: the amount must be between 0 and the most the table has, the price (when there is an amount) between the
// two limits; then the three numbers (amount, price, the kind chosen) go to the table's window (the desktop's, +0x1F4), which
// takes the focus, and this window closes. Otherwise the player is told the allowed range (system messages 0x617 / 0x618)
INT NCManorCropInfoChangeWnd::OnOKBtnClick()
{
    guard(NCManorCropInfoChangeWnd::OnOKBtnClick);
    TCHAR Message[1024];
    INT Amount = appAtoi(*AmountEdit->Left);
    if (Amount >= 0 && MaxAmount >= Amount)
    {
        INT Price = appAtoi(*PriceEdit->Left);
        if (Amount == 0 || (MinPrice <= Price && MaxPrice >= Price))
        {
            const TCHAR* KindText = PriceCombo->GetItemText(PriceCombo->Selected);
            INT Kind = 0;
            if (KindText)
                Kind = appAtoi(KindText);
            NCManorCropInfoSettingWnd* Setting = (NCManorCropInfoSettingWnd*)::nw::Field<NCWnd*>(GConsole->Desktop, 0x1F4);
            Setting->NotifyChangeTomValue(Amount, Price, Kind);
            ::nw::Field<NCWnd*>(GConsole->Desktop, 0x1F4)->SetFocus();
            NCDialogBox::HideModal();
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
NW_HOOK_METHOD(0x1012ff00, NCManorCropInfoChangeWnd, OnOKBtnClick)

// 0x10130260 (vslot 37) - the base's OnCreate, the back texture, the title bar, OK and Cancel, the two edit boxes (ten
// characters), the combo box of the price's kind ("1", "2")
INT NCManorCropInfoChangeWnd::OnCreate()
{
    guard(NCManorCropInfoChangeWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor4_back"));

    FrameCtrl = ManorNewFrameCtrl(1, 0, 0);
    FrameID = (INT)0xFFDCCC20;
    FrameCtrl->ID = (INT)0xFFDCCC20;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    OKButton = ManorNewButton(0, 0);
    OKButton->ID = NID_MANORCHANGE_OK;
    OKButton->SetPosition(0x33, 0xAC, 0, 0);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORCHANGE_CANCEL;
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

    PriceCombo = new NCComboBox();
    PriceCombo->ID = (INT)0xFFDCCC30;
    PriceCombo->SetPosition(0x91, 0x8F, 0x44, 0x11);
    CreateChild(PriceCombo, NWS_VISIBLE | 0x10);
    ManorComboAddTextOnly(PriceCombo, TEXT("1"));
    ManorComboAddTextOnly(PriceCombo, TEXT("2"));
    PriceCombo->SetSelected(0);
    CropName = TEXT("");
    MaxAmount = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130260, NCManorCropInfoChangeWnd, OnCreate)

NW_CHECK_VTABLE(NCManorCropInfoChangeWnd, 0x101ae518, 76)
