/*=============================================================================
    NCPetitionBeginWnd.cpp - the petition form (C3 addresses). Stock: 0x100567c0, 0x10056820, 0x10055640 (the
    categories' array), 0x100d06b0 (WindowProc), 0x100d0750, 0x100d13e0 .. 0x100d1740, 0x100d1f00 .. 0x100d2b5b.
=============================================================================*/
#include "nwindow/NCPetitionBeginWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NCPetitionWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NWndCreate.h"

// ids (names ours)
enum
{
    NID_PETBEGIN_COMBO = 0xF0E0A1,
    NID_PETBEGIN_OK = 0xF0E0A2,
    NID_PETBEGIN_CANCEL = 0xF0E0A3,
    NID_PETBEGIN_EDIT = 0xF0E0A4,
    NID_PETBEGIN_FRAME = 0xF0A0B5,
};
// system messages the form tells (names ours) and the box's id (the plain notice)
enum
{
    MSG_PetitionNoKind = 0x324,
    MSG_PetitionTooShort = 0x182,
    MSG_PetitionTooLong = 0x3CB,
    DIALOG_Notice = 0x1F,
};
// the sysstring ids of the buttons, the kinds and the three texts the panel shows (names ours)
enum
{
    SYSSTR_PetitionOK = 694,
    SYSSTR_PetitionCancel = 473,
    SYSSTR_PetitionKindFirst = 695,    // the combo's first line (the kinds follow from localization.ini)
    SYSSTR_PetitionKindLabel = 705,
    SYSSTR_PetitionTextLabel = 707,
    SYSSTR_PetitionSender = 708,       // "<sender> : <text>" in the conversation's first line
    SYSSTR_PetitionHelpLabel = 412,
};

// 0x1005dde0 - the categories' array goes (the stock code keeps one instance per class that holds such an array)
typedef TArray<NPetitionCategory> TPetitionCategoryArray;
NW_HOOK_DTOR(0x1005dde0, TPetitionCategoryArray)

// 0x1005f370 - NCWnd's constructor, the vtable and the empty array of categories
NCPetitionBeginWnd::NCPetitionBeginWnd()
{
}
NW_HOOK_CTOR(0x1005f370, NCPetitionBeginWnd)

// 0x1005f3d0
NW_HOOK_DTOR(0x1005f3d0, NCPetitionBeginWnd)

// 0x100e00d0 (vslot 25) - OK (68), Cancel (69) and the combo (slot 70, with the notification code)
INT NCPetitionBeginWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_PETBEGIN_OK)
            return OnSend() != 0;
        if (wParam == NID_PETBEGIN_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_PETBEGIN_COMBO)
            return ::nw::VirtualCall<INT>(this, 76, (INT)lParam) != 0;   // OnTypeChosen (the slot)
    }
    return 0;
}
NW_HOOK_METHOD(0x100e00d0, NCPetitionBeginWnd, WindowProc)

// 0x100e0df0 (slot 76) - the kind chosen (1 .. the number of categories): its help page is loaded from "..\L2text\<Link>"
INT NCPetitionBeginWnd::OnTypeChosen(INT Type)
{
    TCHAR Path[256];
    if (Type >= 1 && Type <= Categories.Num())
    {
        appSprintf(Path, TEXT("..\\L2text\\%s"), Categories(Type - 1).Link);
        HelpViewer->LoadHtmlFile(Path);
    }
    return 1;
}
NW_HOOK_METHOD(0x100e0df0, NCPetitionBeginWnd, OnTypeChosen)

// 0x100e0170 - kind none, the text emptied, the help page cleared
void NCPetitionBeginWnd::ResetForm()
{
    TypeCombo->SetSelected(0);
    TextEdit->Clear();
    HelpViewer->Clear();
}
NW_HOOK_METHOD(0x100e0170, NCPetitionBeginWnd, ResetForm)

// NCComboBox::AddItem(INT SysStringID) in this client is the anonymous 0x10006800
static FORCEINLINE void ComboAddItem(NCComboBox* Combo, INT SysStringID)
{
    Combo->AddItem(SysStringID);
}

// 0x100e22a0 (sub_100d1f00 in C3; name ours) - [CategoryNum] Num of localization.ini and for each n [Category<n>]: its Title
// (a sysstring id) goes into the combo, its Link and the id into Categories. A category without Title or Link ends the
// list (the loop is left, the ones read stay)
void NCPetitionBeginWnd::LoadCategories()
{
    INT Num = 0;
    GConfig->GetInt(TEXT("CategoryNum"), TEXT("Num"), Num, TEXT("localization.ini"));
    for (INT i = 0; i < Num; i++)
    {
        TCHAR Section[128];
        INT Title;
        TCHAR LinkRaw[256];
        NPetitionCategory Entry;
        appSprintf(Section, TEXT("Category%d"), i + 1);
        if (!GConfig->GetInt(Section, TEXT("Title"), Title, TEXT("localization.ini")))
            break;
        if (!GConfig->GetString(Section, TEXT("Link"), LinkRaw, 256, TEXT("localization.ini")))
            break;
        Entry.TitleID = Title;
        appStrncpy(Entry.Link, LinkRaw, 256);
        ComboAddItem(TypeCombo, Title);
        Categories.AddItem(Entry);
    }
}
NW_HOOK_METHOD(0x100e22a0, NCPetitionBeginWnd, LoadCategories)

// 0x100e2b50 (vslot 37) - the panel's texture, the title bar, the kind combo (the first line is sysstring 695), the help
// page under it, the text edit and the OK / Cancel buttons (694 / 473)
INT NCPetitionBeginWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PetitionWnd.petition1_back"));
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_PETBEGIN_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    TypeCombo = NewWindowOf<NCComboBox>(0x10006A10);
    TypeCombo->ID = NID_PETBEGIN_COMBO;
    TypeCombo->SetPosition(51, 50, 145, 17);
    CreateChild(TypeCombo, NWS_VISIBLE);
    HelpViewer = NewWindowOf<NCHtmlViewer>(0x1003EDF0);
    HelpViewer->SetPosition(8, 79, 240, 141);
    CreateChild(HelpViewer, NWS_VISIBLE);
    TextEdit = NewWindowOf<NCMultiEdit>(0x1002A040);
    TextEdit->ID = NID_PETBEGIN_EDIT;
    TextEdit->SetPosition(8, 241, 240, 124);
    CreateChild(TextEdit, NWS_VISIBLE);
    OKButton = NewButton(TEXT("L2UI_ch3.Button.btn1_normal"), TEXT("L2UI_ch3.Button.btn1_normalon"), 0);
    OKButton->ID = NID_PETBEGIN_OK;
    OKButton->SetPosition(51, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(SYSSTR_PetitionOK);
    CancelButton = NewButton(TEXT("L2UI_ch3.Button.btn1_normal"), TEXT("L2UI_ch3.Button.btn1_normalon"), 0);
    CancelButton->ID = NID_PETBEGIN_CANCEL;
    CancelButton->SetPosition(131, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(SYSSTR_PetitionCancel);
    ComboAddItem(TypeCombo, SYSSTR_PetitionKindFirst);
    LoadCategories();
    return 1;
}
NW_HOOK_METHOD(0x100e2b50, NCPetitionBeginWnd, OnCreate)

// 0x100e23d0 (vslot 38) - the categories' array is emptied first, then the buttons, the help page, the combo and the
// edit go (the title bar is left alone)
INT NCPetitionBeginWnd::OnDestroy()
{
    guard(NCPetitionBeginWnd::OnDestroy);
    Categories.Empty();
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, HelpViewer);
    DestroyChild(this, TypeCombo);
    DestroyChild(this, TextEdit);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100e23d0, NCPetitionBeginWnd, OnDestroy)

// 0x100e0af0 (vslot 56) - the panel (the window less the title bar) under the title bar and the three labels (the texture
// is the window's size; the labels are in 0xFFDCDCDC)
INT NCPetitionBeginWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT H = (INT)(Height - 20.0f);
    INT W = (INT)Width;
    Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    Device->DrawText(22, 30, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_PetitionKindLabel], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    Device->DrawText(22, 53, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_PetitionHelpLabel], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    Device->DrawText(22, 226, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_PetitionTextLabel], 0, 0, 0, L2FONT_Normal, 0,
                     0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100e0af0, NCPetitionBeginWnd, OnPaint)

// the conversation window the console keeps (+0x3C00 in this client; NConsoleWnd.h puts it at +0x3C04) and its AddMessage
// (0x100DFC60 here; the header has C3's address)
static FORCEINLINE NCWnd* PetitionConversation()
{
    return ::nw::Field<NCWnd*>(GConsole, 0x3C00);
}

// 0x100e0c40 (vslot 74) - OK: a kind must be chosen and the text be 5 to 254 characters; then the petition goes out, the
// conversation window starts with the player's own line and this form is emptied and closed. Else a notice box tells what
// is wrong (the form stays). The line is in 0xFFDCDCDC
INT NCPetitionBeginWnd::OnSend()
{
    INT Type = TypeCombo->Selected;
    NDialogInfo Info;
    INT MsgID;
    if (Type == 0)
        MsgID = MSG_PetitionNoKind;
    else
    {
        TCHAR* Text = TextEdit->GetText();
        if (Text && appStrlen(Text) >= 5)
        {
            if (appStrlen(Text) >= 255)
                MsgID = MSG_PetitionTooLong;
            else
            {
                GConsole->RequestPetition(Text, Type);
                ((NCPetitionWnd*)PetitionConversation())->Clear();
                TCHAR Line[516];
                appSprintf(Line, TEXT("%s : %s"), *GL2GameData.SysString[SYSSTR_PetitionSender], Text);
                ((NCPetitionWnd*)PetitionConversation())->AddMessage(Line, 0xFFDCDCDC, 0, 0);   // 0x100DFC60 (the same body as 0x100DFE90)
                ResetForm();
                HideWindow();
                return 1;
            }
        }
        else
            MsgID = MSG_PetitionTooShort;
    }
    Info.SetSystemMessage(MsgID);
    Info.ID = DIALOG_Notice;
    Info.bMainBox = 1;
    Info.bInput = 0;
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x100e0c40, NCPetitionBeginWnd, OnSend)

// 0x100e0dc0 (vslot 75) - Cancel: a petition already open is withdrawn; the form is emptied and closed
INT NCPetitionBeginWnd::OnCancel()
{
    GConsole->RequestPetitionCancel();
    ResetForm();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100e0dc0, NCPetitionBeginWnd, OnCancel)

NW_CHECK_VTABLE(NCPetitionBeginWnd, 0x101adb98, 77)
