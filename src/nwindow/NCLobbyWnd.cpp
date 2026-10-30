/*=============================================================================
    NCLobbyWnd.cpp - the character selection screen. Stock (C3): 0x1008f600 .. 0x1008f95b,
    0x10090650 .. 0x10090b19 (NCPawnSelectWnd's functions sit in between),
    0x10090d60 .. 0x1009116e.
=============================================================================*/
#include "nwindow/NCLobbyWnd.h"
#include "nwindow/NCPawnSelectWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NWndUtil.h"

// NWM_COMMAND id handled by WindowProc (the buttons are in the two windows at the bottom now)
enum
{
    NID_LOBBY_10001 = 0x10001,
};

// systemmsg lines (names ours)
enum
{
    MSG_TooManyCharacters = 77,    // "You cannot create another character. ..."
    MSG_DeleteCharacter = 78,      // "Do you want to delete $s1?"
    MSG_BadCharacterName = 205,    // "Incorrect character name. Please ask the GM."
    MSG_DaysLeft = 1185,           // the unit written after the days left until the deletion
};

// dialog id of the delete question: only its OK comes back (NWM_DIALOG_RESULT)
enum { DIALOG_DeleteCharacter = 0 };

// the characters of the stage as the console keeps them (UL2ConsoleWnd +0x27AC in C3, eight User
// records)
static User* ConsoleCharacters() { return GConsole->SelectCharacterInfo; }

// the two fields of a character record that C4 split off the old DeleteDays word (C3: the low 16 bits the days, bit 16 the lock):
// the days left until the deletion is an int at +0x160 and the lock a byte at +0x15C of the record (OnStart / OnDelete / OnPaint
// read them at PawnSelectWnd + 0x100 + 0x248 * i + 0x160 / + 0x15C; the shared User header still names +0x15C DeleteDays)
static FORCEINLINE INT CharDeleteDays(User& Char)
{
    return *(INT*)((BYTE*)&Char + 0x160);
}
static FORCEINLINE BYTE CharLocked(User& Char)
{
    return *((BYTE*)&Char + 0x15C);
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1009cf10 - the chat texture is looked up here; the panel, the boxes and the windows are left as allocated
NCLobbyWnd::NCLobbyWnd()
{
    NumCharacters = 0;
    ChatTex = FindTexture(TEXT("NWindow.Chat"));
}
NW_HOOK_CTOR(0x1009cf10, NCLobbyWnd)

// 0x1009cfa0
NW_HOOK_DTOR(0x1009cfa0, NCLobbyWnd)

// 0x1009b840 (vslot 25) - the delete question's answer arrives as NWM_DIALOG_RESULT; of the commands only 0x10001
// comes here (the buttons are in the two windows below, which call the virtuals 70 .. 73)
INT NCLobbyWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_LOBBY_10001)
            return OnCommand10001() != 0;
    }
    else if (Msg == NWM_DIALOG_RESULT)
        return OnDialogResult((INT)wParam, (INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1009b840, NCLobbyWnd, WindowProc)

// 0x1009cff0 (vslot 37) - the panel in the upper left corner (its title is SysString 157, set through the virtual SetTitleID), the
// question box centered, the "Start" window and the function window (the buttons) at the bottom (C3: NCLobbyStartWnd and
// NCLobbyFunctionWnd, classes of their own). The constructors are the stock's: 0x1009cdf0, 0x1000b1a0, 0x1009b500, 0x1009b070
INT NCLobbyWnd::OnCreate()
{
    NCWnd::OnCreate();

    PawnSelectWnd = new NCPawnSelectWnd();
    PawnSelectWnd->SetPosition(24, 27, 256, 108);
    CreateChild(PawnSelectWnd, 0x10000 | NWS_NOCLOSE | NWS_VISIBLE);
    ::nw::VirtualCall<void>(PawnSelectWnd, 56, 157);           // NCWnd::SetTitleID: "Select Character"
    PawnSelectWnd->SetAnchor(1, 24, 27);

    DialogBox = new NCGaraDialogBox();         // the question box
    DialogBox->SetPosition((INT)(Width * 0.5f - 101.0f), (INT)(Height * 0.5f - 44.0f), 203, 87);
    CreateChild(DialogBox, 0x20 | NWS_VISIBLE);
    DialogBox->SetButtonTexts(184, 185);                       // "Yes", "No"
    DialogBox->HideWindow();

    StartWnd = new NCLobbyStartWnd();
    StartWnd->SetPosition(GSEKScreenX / 2 - 71, GSEKScreenY - 111, 140, 49);
    CreateChild(StartWnd, NWS_VISIBLE);
    StartWnd->SetAnchor(8, 0, -10);

    FunctionWnd = new NCLobbyFunctionWnd();
    FunctionWnd->SetPosition(GSEKScreenX - 154, GSEKScreenY - 200, 120, 138);
    CreateChild(FunctionWnd, NWS_VISIBLE);
    FunctionWnd->SetAnchor(9, -34, -10);
    return 1;
}
NW_HOOK_METHOD(0x1009cff0, NCLobbyWnd, OnCreate)

// 0x1009ca60 (vslot 38) - C4: the two windows at the bottom go too; then NCWnd's
INT NCLobbyWnd::OnDestroy()
{
    guard(NCLobbyWnd::OnDestroy);
    if (PawnSelectWnd)
    {
        RemoveChildWindow(PawnSelectWnd->Entry);
        delete PawnSelectWnd;
        PawnSelectWnd = NULL;
    }
    if (DialogBox)
    {
        RemoveChildWindow(DialogBox->Entry);
        delete DialogBox;
        DialogBox = NULL;
    }
    if (StartWnd)
    {
        RemoveChildWindow(StartWnd->Entry);
        delete StartWnd;
        StartWnd = NULL;
    }
    if (FunctionWnd)
    {
        RemoveChildWindow(FunctionWnd->Entry);
        delete FunctionWnd;
        FunctionWnd = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009ca60, NCLobbyWnd, OnDestroy)

// 0x1009bb20 (vslot 48) - nothing chosen yet: the selection cleared and the buttons enabled
void NCLobbyWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    GConsole->SelectedCharacterNum(-1);
    PawnSelectWnd->NameCombo->SetSelected(-1);
    PawnSelectWnd->Selected = -1;
    FunctionWnd->CreateButton->EnableWindow(1);
    FunctionWnd->DeleteButton->EnableWindow(1);
    StartWnd->StartButton->EnableWindow(1);
    DialogBox->HideWindow();
}
NW_HOOK_METHOD(0x1009bb20, NCLobbyWnd, ShowWindow)

// 0x1009c7e0 (vslot 62) - each character's name over its head: half its collision height above the location, plus 7 (4 more for
// mesh type 0), white, reddened by its karma; a character marked for deletion (days left, and not locked) also gets under the
// name the time left (C4: "<SysString 1108> : <time>" with NWndUtil::MakeTimeStr of the days field). Every record is copied
// whole first, as in the stock code. The clip goes on before BeginPaint.
INT NCLobbyWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLobbyWnd::OnPaint);
    TCHAR Days[512];
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    for (INT i = 0; i < 8; i++)
    {
        User Info = ConsoleCharacters()[i];
        if (Info.Pawn)
        {
            FVector Loc = Info.Pawn->Location;
            FLOAT Z = Info.Pawn->CollisionHeight * 0.5f + Loc.Z + 7.f;
            if (Info.GetMeshType() == 0)
                Z += 4.f;
            INT Red = 0;
            if (Info.Karma > 255)
                Red = 255;
            if (Info.Karma > 0)
            {
                Red = Info.Karma / 16 + 100;
                if (Info.Karma >= Red)
                {
                    Red = Info.Karma;
                    if (Info.Karma >= 255)
                        Red = 255;
                }
            }
            DWORD Color = ((~Red) << 8 | (255 - Red)) | 0xFFFF0000;
            Device->Draw3DCoordText(FVector(Loc.X, Loc.Y, Z), Color, Info.GetName(), NULL, 2, 0, (L2FontType)1);
            INT DeleteDays = CharDeleteDays(Info);
            if (DeleteDays > 0 && !CharLocked(Info))
            {
                Z += 9.0f;
                FString Left = NWndUtil::MakeTimeStr(DeleteDays);
                appSprintf(Days, TEXT("%s : %s"), *GL2GameData.SysString[1108], *Left);
                // NCGDevice::DrawChatting (0x10013140): the same arguments as the text's, the color first
                Device->DrawChatting(FVector(Loc.X, Loc.Y, Z), (DWORD)0xFFDCDCDC, (TCHAR*)Days, 2, 0, (L2FontType)0, 0, 1, 0);
            }
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009c7e0, NCLobbyWnd, OnPaint)

// 0x1009bbb0 (vslot 65) - Enter is a click of "Start"; the base class handles the key as well
INT NCLobbyWnd::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN)
        OnStartClick();
    NCWnd::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x1009bbb0, NCLobbyWnd, OnKeyDown)

// 0x1009b8e0 - enter the world with the chosen character, unless it is marked for deletion (days left) or locked, or its name
// has characters the font cannot draw (the message of system message 205); the intro music stops (the console's virtual 346)
void NCLobbyWnd::OnStart()
{
    INT Index = PawnSelectWnd->GetSelected();
    if (Index >= 0)
    {
        User& Char = PawnSelectWnd->Characters[Index];
        if (CharDeleteDays(Char) <= 0 && CharLocked(Char) == 0 && NumCharacters > 0 && Index < NumCharacters)
        {
            const TCHAR* Name = PawnSelectWnd->NameCombo->GetItemText(Index);
            if (!Name || !*Name || !GConsole->HasInvalidChars((TCHAR*)Name))
            {
                GConsole->RequestGameStart(Index);
                ::nw::VirtualCall<void>(GConsole, 346);
            }
            else
                GConsole->ShowMessageWnd(*GL2GameData.SystemMsg[MSG_BadCharacterName].Message);
        }
    }
}
NW_HOOK_METHOD(0x1009b8e0, NCLobbyWnd, OnStart)

// 0x1009ba60 (vslot 75) - the delete question's answer (NWM_DIALOG_RESULT): 0 cancelled, the selection is cleared and
// the buttons enabled; else the chosen character is deleted
INT NCLobbyWnd::OnDialogResult(INT bYes, INT Unused)
{
    if (bYes == 0)
    {
        GConsole->SelectedCharacterNum(-1);
        PawnSelectWnd->NameCombo->SetSelected(-1);
        PawnSelectWnd->Selected = -1;
        FunctionWnd->CreateButton->EnableWindow(1);
        FunctionWnd->DeleteButton->EnableWindow(1);
        StartWnd->StartButton->EnableWindow(1);
    }
    else
    {
        INT Index = PawnSelectWnd->GetSelected();
        if (Index >= 0)
            GConsole->RequestCharacterDelete(Index);
    }
    return 1;
}
NW_HOOK_METHOD(0x1009ba60, NCLobbyWnd, OnDialogResult)

// 0x1009b990 (vslot 76) - the "Start" button: the voices are stopped (fade 1.5), then the character starts
INT NCLobbyWnd::OnStartClick()
{
    if (GConsole && GConsole->Engine && GConsole->Engine->Audio)
        GConsole->Engine->Audio->StopAllVoice(1.5f, 0);
    OnStart();
    return 1;
}
NW_HOOK_METHOD(0x1009b990, NCLobbyWnd, OnStartClick)

// 0x1009b9d0 (vslot 77) - the "Previous" button: the voices are stopped, then back to the login
INT NCLobbyWnd::OnPreviousClick()
{
    if (GConsole && GConsole->Engine && GConsole->Engine->Audio)
        GConsole->Engine->Audio->StopAllVoice(1.5f, 0);
    GConsole->ReturnToLogin();
    return 1;
}
NW_HOOK_METHOD(0x1009b9d0, NCLobbyWnd, OnPreviousClick)

// 0x1009ba10 (vslot 78) - seven characters at most
INT NCLobbyWnd::OnCreateCharacter()
{
    if (NumCharacters == 7)
    {
        GConsole->ShowMessageWnd(*GL2GameData.SystemMsg[MSG_TooManyCharacters].Message);
        return 1;
    }
    if (NumCharacters < 7)
        GConsole->RequestNewCharacter();
    return 1;
}
NW_HOOK_METHOD(0x1009ba10, NCLobbyWnd, OnCreateCharacter)

// 0x1009cb90 (vslot 79) - "Do you want to delete ...?" (a character with days left, or a locked one, gets no question)
INT NCLobbyWnd::OnDelete()
{
    TCHAR Name[32];
    TCHAR Text[128];
    INT Index = PawnSelectWnd->GetSelected();
    if (Index >= 0)
    {
        User& Char = PawnSelectWnd->Characters[Index];
        if (CharDeleteDays(Char) <= 0 && CharLocked(Char) == 0)
        {
            appSprintf(Name, TEXT("%s"), PawnSelectWnd->GetCharacterName(PawnSelectWnd->GetSelected()));
            GConsole->FormatSystemMessage(Text, *GL2GameData.SystemMsg[MSG_DeleteCharacter].Message, Name, NULL);
            NDialogInfo Info;
            Info.ID = DIALOG_DeleteCharacter;
            Info.bMainBox = 0;
            Info.bInput = 0;
            Info.Owner = Entry;
            Info.SetText(Text);
            DialogBox->SetInfo(&Info);
            DialogBox->ShowWindow();
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1009cb90, NCLobbyWnd, OnDelete)

// 0x1009c790 - one character of the server's list (NConsoleWnd); the record by value (ret 0x248)
INT NCLobbyWnd::AddCharacter(User Info)
{
    if (NumCharacters >= 7)
        return 0;
    PawnSelectWnd->AddCharacter(Info);
    NumCharacters++;
    return 1;
}
NW_HOOK_METHOD(0x1009c790, NCLobbyWnd, AddCharacter)

// 0x1009b8c0 - NConsoleWnd::ReturnToLogin and before a new list
void NCLobbyWnd::ClearCharacters()
{
    PawnSelectWnd->ClearCharacters();
    NumCharacters = 0;
}
NW_HOOK_METHOD(0x1009b8c0, NCLobbyWnd, ClearCharacters)

NW_CHECK_VTABLE(NCLobbyWnd, 0x101b7e78, 80)

/*-----------------------------------------------------------------------------
    NCLobbyStartWnd (C3).
-----------------------------------------------------------------------------*/

// NWM_COMMAND ids of the two windows' buttons (names ours)
enum
{
    NID_LOBBY_START = 0x1009,          // NCLobbyStartWnd's button
    NID_LOBBY_PREVIOUS = 0x100A,       // NCLobbyFunctionWnd's
    NID_LOBBY_CREATE = 0x100B,
    NID_LOBBY_DELETE = 0x100C,
};

// 0x1009b500
NCLobbyStartWnd::NCLobbyStartWnd()
{
}
NW_HOOK_CTOR(0x1009b500, NCLobbyStartWnd)

// 0x1009b550
NW_HOOK_DTOR(0x1009b550, NCLobbyStartWnd)

// 0x1009b120 (vslot 25)
INT NCLobbyStartWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_LOBBY_START)
        return OnGameStartBtnClick() != 0;
    return 0;
}
NW_HOOK_METHOD(0x1009b120, NCLobbyStartWnd, WindowProc)

// 0x1009b3a0 (vslot 37) - the button (disabled until a character is chosen) and its backdrop; NCWnd::OnCreate is not called
INT NCLobbyStartWnd::OnCreate()
{
    guard(NCLobbyStartWnd::OnCreate);
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.lobby_startbtnback"), 1);

    StartButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton2"), TEXT("L2UI_CH3.Button.BigButton2_down"), 0);
    StartButton->ID = NID_LOBBY_START;
    StartButton->SetPosition(13, 10, 116, 31);
    CreateChild(StartButton, NWS_VISIBLE);
    StartButton->SetTextID(160);                                // "Start"
    StartButton->EnableWindow(0);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009b3a0, NCLobbyStartWnd, OnCreate)

// 0x1009b180 (vslot 38) - the button goes; NCWnd::OnDestroy is not called (as in the stock code)
INT NCLobbyStartWnd::OnDestroy()
{
    guard(NCLobbyStartWnd::OnDestroy);
    if (StartButton)
    {
        RemoveChildWindow(StartButton->Entry);
        delete StartButton;
        StartButton = NULL;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009b180, NCLobbyStartWnd, OnDestroy)

// 0x1009b220 (vslot 62) - the backdrop (a 140 x 49 piece) stretched over the window
INT NCLobbyStartWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLobbyStartWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, 140, 49, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009b220, NCLobbyStartWnd, OnPaint)

// 0x1009b310 (vslot 74) - the lobby's slot 70 (OnStartClick); the cast is checked
INT NCLobbyStartWnd::OnGameStartBtnClick()
{
    guard(NCLobbyStartWnd::OnGameStartBtnClick);
    CastChecked<NCLobbyWnd>(Entry->Parent->Wnd)->OnStartClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009b310, NCLobbyStartWnd, OnGameStartBtnClick)

NW_CHECK_VTABLE(NCLobbyStartWnd, 0x101b78b0, 75)

/*-----------------------------------------------------------------------------
    NCLobbyFunctionWnd (C3).
-----------------------------------------------------------------------------*/

// 0x1009b070
NCLobbyFunctionWnd::NCLobbyFunctionWnd()
{
}
NW_HOOK_CTOR(0x1009b070, NCLobbyFunctionWnd)

// 0x1009b0c0
NW_HOOK_DTOR(0x1009b0c0, NCLobbyFunctionWnd)

// 0x1009a900 (vslot 25)
INT NCLobbyFunctionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_LOBBY_PREVIOUS)
            return OnGameCancelBtnClick() != 0;
        if (wParam == NID_LOBBY_CREATE)
            return OnNewCharacterBtnClick() != 0;
        if (wParam == NID_LOBBY_DELETE)
            return OnDelCharacterBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1009a900, NCLobbyFunctionWnd, WindowProc)

// 0x1009ada0 (vslot 37) - the backdrop and the buttons "Create" and "Delete" (disabled until a character is chosen) above the
// one of sysstring 884; NCWnd::OnCreate is called
INT NCLobbyFunctionWnd::OnCreate()
{
    guard(NCLobbyFunctionWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.lobby_menuback"), 1);

    CreateButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"), TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    CreateButton->ID = NID_LOBBY_CREATE;
    CreateButton->SetPosition(13, 20, 96, 23);
    CreateChild(CreateButton, NWS_VISIBLE);
    CreateButton->SetTextID(158);                               // "Create"

    DeleteButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"), TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    DeleteButton->ID = NID_LOBBY_DELETE;
    DeleteButton->SetPosition(13, 45, 96, 23);
    CreateChild(DeleteButton, NWS_VISIBLE);
    DeleteButton->SetTextID(159);                               // "Delete"
    DeleteButton->EnableWindow(0);

    PreviousButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"), TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    PreviousButton->ID = NID_LOBBY_PREVIOUS;
    PreviousButton->SetPosition(13, 95, 96, 23);
    CreateChild(PreviousButton, NWS_VISIBLE);
    PreviousButton->SetTextID(884);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009ada0, NCLobbyFunctionWnd, OnCreate)

// 0x1009a9a0 (vslot 38) - the three buttons go; C4: and then NCWnd's OnDestroy
INT NCLobbyFunctionWnd::OnDestroy()
{
    guard(NCLobbyFunctionWnd::OnDestroy);
    if (PreviousButton)
    {
        RemoveChildWindow(PreviousButton->Entry);
        delete PreviousButton;
        PreviousButton = NULL;
    }
    if (CreateButton)
    {
        RemoveChildWindow(CreateButton->Entry);
        delete CreateButton;
        CreateButton = NULL;
    }
    if (DeleteButton)
    {
        RemoveChildWindow(DeleteButton->Entry);
        delete DeleteButton;
        DeleteButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009a9a0, NCLobbyFunctionWnd, OnDestroy)

// 0x1009aaa0 (vslot 62) - the backdrop (a 120 x 138 piece) stretched over the window
INT NCLobbyFunctionWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLobbyFunctionWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, 120, 138, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009aaa0, NCLobbyFunctionWnd, OnPaint)

// 0x1009abf0 (vslot 74) - the lobby's slot 71 (OnPreviousClick); the cast is checked
INT NCLobbyFunctionWnd::OnGameCancelBtnClick()
{
    guard(NCLobbyFunctionWnd::OnGameCancelBtnClick);
    CastChecked<NCLobbyWnd>(Entry->Parent->Wnd)->OnPreviousClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009abf0, NCLobbyFunctionWnd, OnGameCancelBtnClick)

// 0x1009ac80 (vslot 75) - the lobby's slot 72 (OnCreateCharacter)
INT NCLobbyFunctionWnd::OnNewCharacterBtnClick()
{
    guard(NCLobbyFunctionWnd::OnNewCharacterBtnClick);
    CastChecked<NCLobbyWnd>(Entry->Parent->Wnd)->OnCreateCharacter();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009ac80, NCLobbyFunctionWnd, OnNewCharacterBtnClick)

// 0x1009ad10 (vslot 76) - the lobby's slot 73 (OnDelete)
INT NCLobbyFunctionWnd::OnDelCharacterBtnClick()
{
    guard(NCLobbyFunctionWnd::OnDelCharacterBtnClick);
    CastChecked<NCLobbyWnd>(Entry->Parent->Wnd)->OnDelete();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009ad10, NCLobbyFunctionWnd, OnDelCharacterBtnClick)

// 0x1009ab90 - CastChecked<NCLobbyWnd>, the stock's out-of-line instance (the function and start windows' buttons)
static NCLobbyWnd* CastCheckedNCLobbyWnd(UObject* Object)
{
    return CastChecked<NCLobbyWnd>(Object);
}
NW_HOOK_FUNC(0x1009ab90, CastCheckedNCLobbyWnd)

NW_CHECK_VTABLE(NCLobbyFunctionWnd, 0x101b75a0, 77)
