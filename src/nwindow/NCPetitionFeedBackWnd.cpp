/*=============================================================================
    NCPetitionFeedBackWnd.cpp - the petition feedback form (C3 only; C3 addresses). Stock: 0x10077a10, 0x10077a60,
    0x100d0780 .. 0x100d0870, 0x100d1740 .. 0x100d1c4a (the paint, the options, OnSend), 0x100d21b0 .. 0x100d2753
    (OnCreate), and two helpers: the check boxes' textures 0x10004650 and the console's request 0x100611a0.
=============================================================================*/
#include "nwindow/NCPetitionFeedBackWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCPetitionWnd.h"
#include "nwindow/MsnUiStock.h"   // NCCheckBoxTexturesView
#include "nwindow/NWndCreate.h"

// the sysstring ids (names ours): the options' labels, the OK button and the two texts of the form
enum
{
    SYSSTR_FeedBackOption1 = 850,      // .. 854
    SYSSTR_FeedBackOK = 140,
    SYSSTR_FeedBackLabel1 = 855,
    SYSSTR_FeedBackLabel2 = 856,
    MSG_FeedBackTooLong = 0x147,
};

// 0x10005290 (sub_10004650 in C3; a method of NCCheckBox, ret 0x10; also AddedConfirmDialog::OnCreate's) - the four textures
// by name (checked, unchecked, unchecked unable, checked unable) at +0x274 .. +0x280 (NCheckBox.h has them at +0x1F4 .. in
// the C3 order, before the tool tip record this client made larger); a name left NULL leaves its texture as it is.
// Declared on a view of NCCheckBox (its header is not this lane's): NCCheckBoxTexturesView, MsnUiStock.h (the one definition of the struct;
// the command channel window's file and AddedConfirmDialog.cpp call it from there)
void NCCheckBoxTexturesView::SetTextures(const TCHAR* Checked, const TCHAR* Unchecked, const TCHAR* UncheckedDisabled,
                                         const TCHAR* CheckedDisabled)
{
    if (Checked)
        ::nw::Field<UTexture*>(this, 0x274) = FindTexture(Checked);
    if (Unchecked)
        ::nw::Field<UTexture*>(this, 0x278) = FindTexture(Unchecked);
    if (UncheckedDisabled)
        ::nw::Field<UTexture*>(this, 0x27C) = FindTexture(UncheckedDisabled);
    if (CheckedDisabled)
        ::nw::Field<UTexture*>(this, 0x280) = FindTexture(CheckedDisabled);
}
NW_HOOK_METHOD(0x10005290, NCCheckBoxTexturesView, SetTextures)

// 0x1006caa0 (sub_100611a0 in C3, name ours; ret 4) - NetHandler slot 271 (0x43C; C3: 262, 0x418): the feedback of a petition
// 0x100611a0 (sub_100611a0, a method of the console, ret 4) - NetHandler 0x418 (slot 262): the feedback of a petition
// (name ours). Declared on a view of NConsoleWnd (its header is not this lane's)
struct NConsoleWndFeedBackView : public NConsoleWnd
{
    __declspec(noinline) void RequestPetitionFeedBack(L2ParamStack& Params)
    {
        nw::VirtualCall<void>(Engine->NetHandler, 0x43C / 4, &Params);
    }
};
NW_HOOK_METHOD(0x1006caa0, NConsoleWndFeedBackView, RequestPetitionFeedBack)

// 0x10082450 - NCWnd's constructor and the vtable
NCPetitionFeedBackWnd::NCPetitionFeedBackWnd()
{
}
NW_HOOK_CTOR(0x10082450, NCPetitionFeedBackWnd)

// 0x100824a0
NW_HOOK_DTOR(0x100824a0, NCPetitionFeedBackWnd)

// 0x100e01a0 (vslot 25) - the five options (68 .. 72) and OK (73)
INT NCPetitionFeedBackWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_PETFB_OPTION1)
            return OnOption1() != 0;
        if (wParam == NID_PETFB_OPTION2)
            return OnOption2() != 0;
        if (wParam == NID_PETFB_OPTION3)
            return OnOption3() != 0;
        if (wParam == NID_PETFB_OPTION4)
            return OnOption4() != 0;
        if (wParam == NID_PETFB_OPTION5)
            return OnOption5() != 0;
        if (wParam == NID_PETFB_OK)
            return OnSend() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100e01a0, NCPetitionFeedBackWnd, WindowProc)

// a check box of the window (repeated inline in the stock OnCreate): 100 x 12 at x 21. The constructor 0x100058C0 takes four
// arguments in this client (the label's sysstring and three zeros, ret 0x10)
static FORCEINLINE NCCheckBox* CreateOption(NCWnd* Parent, INT TextID, INT ID, INT Y)
{
    NCCheckBox* Box = new NCCheckBox(TextID, 0, 0, 0);   // the constructor 0x100058C0 on the object `new` allocates
    Box->ID = ID;
    Box->SetPosition(21, Y, 100, 12);
    Parent->CreateChild(Box, NWS_VISIBLE);
    static_cast<NCCheckBoxTexturesView*>(Box)->SetTextures(TEXT("L2UI_CH3.RadioCtrl.RadioButton2"),
                                                           TEXT("L2UI_CH3.RadioCtrl.RadioButton1"), NULL, NULL);
    return Box;
}

// 0x100e2550 (vslot 37) - the background, the comment edit, the five options with the radio button textures (the third is
// the one chosen), the title bar (21 high) and OK
INT NCPetitionFeedBackWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.PetitionWnd.petition_fb_back"));
    TextEdit = NewWindowOf<NCMultiEdit>(0x1002A040);
    TextEdit->ID = NID_PETFB_EDIT;
    TextEdit->SetPosition(8, 166, 240, 199);
    CreateChild(TextEdit, NWS_VISIBLE);
    Options[0] = CreateOption(this, SYSSTR_FeedBackOption1, NID_PETFB_OPTION1, 52);
    Options[1] = CreateOption(this, SYSSTR_FeedBackOption1 + 1, NID_PETFB_OPTION2, 71);
    Options[2] = CreateOption(this, SYSSTR_FeedBackOption1 + 2, NID_PETFB_OPTION3, 90);
    Options[3] = CreateOption(this, SYSSTR_FeedBackOption1 + 3, NID_PETFB_OPTION4, 109);
    Options[4] = CreateOption(this, SYSSTR_FeedBackOption1 + 4, NID_PETFB_OPTION5, 128);
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_PETFB_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 21);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    OKButton = NewButton(TEXT("L2UI_CH3.BUTTON.Btn1_normal"), TEXT("L2UI_CH3.BUTTON.Btn1_normalon"), 0);
    OKButton->ID = NID_PETFB_OK;
    OKButton->SetPosition(90, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(SYSSTR_FeedBackOK);
    Options[2]->bChecked = 1;
    return 1;
}
NW_HOOK_METHOD(0x100e2550, NCPetitionFeedBackWnd, OnCreate)

// 0x100e0e50 (vslot 38) - the five options, OK and the edit go (the title bar is left alone); no guard
INT NCPetitionFeedBackWnd::OnDestroy()
{
    DestroyChild(this, Options[0]);
    DestroyChild(this, Options[1]);
    DestroyChild(this, Options[2]);
    DestroyChild(this, Options[3]);
    DestroyChild(this, Options[4]);
    DestroyChild(this, OKButton);
    DestroyChild(this, TextEdit);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100e0e50, NCPetitionFeedBackWnd, OnDestroy)

// 0x100e1360 (vslot 49) - the conversation window's "new petition" button is disabled, the text emptied, the third option
// chosen
void NCPetitionFeedBackWnd::HideWindow()
{
    ((NCPetitionWnd*)::nw::Field<NCWnd*>(GConsole, 0x3C00))->NewButton->EnableWindow(0);
    if (TextEdit)
        TextEdit->Clear();
    OnOption3();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100e1360, NCPetitionFeedBackWnd, HideWindow)

// 0x100e0fa0 (vslot 56) - the background under the title bar and the two lines of text (0xFFDCDCDC)
INT NCPetitionFeedBackWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT H = (INT)(Height - 20.0f);
    INT W = (INT)Width;
    Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    Device->DrawText(22, 32, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_FeedBackLabel1], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(22, 151, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_FeedBackLabel2], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100e0fa0, NCPetitionFeedBackWnd, OnPaint)

// 0x100e10c0 .. 0x100d1af0 (vslots 68 .. 72) - only option N is checked (the stock code writes the five flags in turn)
static FORCEINLINE void ChooseOption(NCPetitionFeedBackWnd* Wnd, INT N)
{
    for (INT i = 0; i < 5; i++)
        Wnd->Options[i]->bChecked = (i == N);
}

INT NCPetitionFeedBackWnd::OnOption1()
{
    ChooseOption(this, 0);
    return 1;
}
NW_HOOK_METHOD(0x100e10c0, NCPetitionFeedBackWnd, OnOption1)

INT NCPetitionFeedBackWnd::OnOption2()
{
    ChooseOption(this, 1);
    return 1;
}
NW_HOOK_METHOD(0x100e1110, NCPetitionFeedBackWnd, OnOption2)

INT NCPetitionFeedBackWnd::OnOption3()
{
    ChooseOption(this, 2);
    return 1;
}
NW_HOOK_METHOD(0x100e1160, NCPetitionFeedBackWnd, OnOption3)

INT NCPetitionFeedBackWnd::OnOption4()
{
    ChooseOption(this, 3);
    return 1;
}
NW_HOOK_METHOD(0x100e11b0, NCPetitionFeedBackWnd, OnOption4)

INT NCPetitionFeedBackWnd::OnOption5()
{
    ChooseOption(this, 4);
    return 1;
}
NW_HOOK_METHOD(0x100e1200, NCPetitionFeedBackWnd, OnOption5)

// 0x100e1250 (vslot 79) - OK: the index of the checked option and the text (when it is not empty) go to the server and
// the window hides. A text of more than 512 bytes in the ANSI code page (the conversion buffer's size; the length is
// read as a C string) is refused with system message 0x147 and the form stays
INT NCPetitionFeedBackWnd::OnSend()
{
    L2ParamStack Params(10);
    for (INT i = 0; i < 5; i++)
        if (Options[i]->bChecked)
            Params.PushBack((void*)(INT_PTR)i);
    TCHAR* Text = TextEdit->GetText();
    if (Text)
    {
        char Buffer[0x200];
        WideCharToMultiByte(0, 0, Text, -1, Buffer, 0x200, NULL, NULL);
        INT Length = 0;
        while (Buffer[Length])
            Length++;
        if (Length > 0x200)
        {
            GConsole->ShowSystemMessage(MSG_FeedBackTooLong, 0xFFB09B79);
            return 1;
        }
    }
    Params.PushBack((void*)Text);
    static_cast<NConsoleWndFeedBackView*>(GConsole)->RequestPetitionFeedBack(Params);
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100e1250, NCPetitionFeedBackWnd, OnSend)

NW_CHECK_VTABLE(NCPetitionFeedBackWnd, 0x101b22b0, 80)
