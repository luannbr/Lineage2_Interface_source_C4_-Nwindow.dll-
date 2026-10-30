/*=============================================================================
    NCPawnSelectWnd.cpp - the character panel of the character selection.
    Stock (C3): 0x1008f350 .. 0x1008f537, 0x1008f540 .. 0x1008f5f3 (NCLobbyWnd's functions follow),
    0x1008f9c0 .. 0x10090157, 0x10090190, 0x10090240 .. 0x10090641, 0x10090b20 .. 0x10090cd5
    (ShowWindow is a body the linker folded with other classes).
=============================================================================*/
#include "nwindow/NCPawnSelectWnd.h"
#include "nwindow/NCLobbyWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <wchar.h>

// An object made as the stock code makes it: StaticAllocateObject of its class, then the constructor at Ctor of
// this client (the header constructors of the controls are not the stock ones)
// The standalone has no stock code: the constructor that runs is T's own (hooked at Ctor with the same arguments, so the
// hook build runs the same one through the stock address).
template <class T, class... A> static FORCEINLINE T* NewStock(DWORD Ctor, A... a)
{
#if NW_HOOK_MODE
    void* Mem = T::operator new(sizeof(T));
    return Mem ? ::nw::StockCallAt<T*>(Ctor, Mem, a...) : NULL;
#else
    (void)Ctor;
    return new T(a...);
#endif
}

// NWM_COMMAND id of the name combo box (name ours)
enum
{
    NID_PAWNSELECT_NAME = 0x1F04,
};

// systemmsg line (name ours): the question whether to cancel the deletion (C3: line 180, with the days left in it)
enum { MSG_RestoreCharacter = 1555 };

// dialog id of the restore question: its answer comes back as NWM_DIALOG_RESULT
enum { DIALOG_RestoreCharacter = 1 };

// exp at the start of each level (index = level): the character selection's own copy
#if NW_HOOK_MODE
#define GLobbyExpTable (reinterpret_cast<const INT*>(::nw::Addr(0x101B79FC)))
#else
extern const INT GLobbyExpTable[];
#endif

// the console's character selection
static NCLobbyWnd* ConsoleLobbyWnd() { return GConsole->LobbyWnd; }

// A field of a User record the shared header does not name (the "last used" flag the server sets: +0x21C in C4, +0x20C in C3)
static FORCEINLINE INT UserInt(const User& Info, INT Offset)
{
    return *(const INT*)((const BYTE*)&Info + Offset);
}

// The deletion state of a character record as this client keeps it (the header's User::DeleteDays, +0x15C, is C3's one
// field of two parts): a byte at +0x15C set when the character is locked (the selection neither selects it nor starts the
// game with it) and the days left until its deletion at +0x160 (0: not being deleted)
static FORCEINLINE UBOOL UserLocked(const User& Info) { return *((const BYTE*)&Info + 0x15C) != 0; }
static FORCEINLINE INT UserDeleteDays(const User& Info) { return *(const INT*)((const BYTE*)&Info + 0x160); }

// NCGDevice::DrawSpecialDigit takes an alpha after the text in this client (ret 0x18, the header declares the five C3
// arguments): the digits of the status bars are drawn opaque
static FORCEINLINE void DrawDigit(NCGDevice* Device, UBOOL bLarge, INT X, INT Y, DWORD Color, TCHAR* Text)
{
    Device->DrawSpecialDigit(bLarge, X, Y, Color, Text, 0xFF);   // 0x10013350
}

// The combo box's AddItem(const TCHAR*) (0x100068B0)
static FORCEINLINE void ComboAdd(NCComboBox* Combo, const TCHAR* Text)
{
    Combo->AddItem(Text);
}

// 0x1009cdf0 - the textures and the combo box are looked up in OnCreate; the seven User records are made by the
// compiler
NCPawnSelectWnd::NCPawnSelectWnd()
{
    BackTex = NULL;
    HPTex = NULL;
    MPTex = NULL;
    EXPTex = NULL;
    UnknownA4 = 0;
    NumCharacters = 0;
    Selected = -1;
}
NW_HOOK_CTOR(0x1009cdf0, NCPawnSelectWnd)

// 0x1009cec0
NW_HOOK_DTOR(0x1009cec0, NCPawnSelectWnd)

// 0x1009b5b0 (vslot 25) - the combo box reports the chosen name, the restore question its
// answer (C2 no longer refuses the messages of a disabled window here)
INT NCPawnSelectWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_PAWNSELECT_NAME)
            return OnSelect((INT)lParam) != 0;
    }
    else if (Msg == NWM_DIALOG_RESULT)
        return OnRestoreAnswer((INT)wParam, (INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1009b5b0, NCPawnSelectWnd, WindowProc)

// 0x1009ccc0 (vslot 37) - the textures (the panel's own back and the player status bars), no actor on any slot yet, the
// name combo box under the title
INT NCPawnSelectWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.select1_back"));
    HPTex = FindTexture(TEXT("L2UI_ch3.playerstatuswnd.ps_hpbar"));
    MPTex = FindTexture(TEXT("L2UI_ch3.PlayerStatusWnd.ps_mpbar"));
    EXPTex = FindTexture(TEXT("L2UI_ch3.PlayerStatusWnd.ps_expbar"));
    for (INT i = 0; i < 7; i++)
        Characters[i].Pawn = NULL;
    NameCombo = NewStock<NCComboBox>(0x10006A10);
    NameCombo->ID = NID_PAWNSELECT_NAME;
    NameCombo->SetPosition(46, 13, 138, 17);
    CreateChild(NameCombo, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1009ccc0, NCPawnSelectWnd, OnCreate)

// 0x1009c3d0 (vslot 38)
INT NCPawnSelectWnd::OnDestroy()
{
    if (NameCombo)
    {
        RemoveChildWindow(NameCombo->Entry);
        delete NameCombo;
        NameCombo = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x1009c3d0, NCPawnSelectWnd, OnDestroy)

// stock body shared with NCEditBox's (0x100a5d70)
void NCPawnSelectWnd::ShowWindow()
{
    NCWnd::ShowWindow();
}

// 0x1009bbe0 (vslot 62) - the back, the labels, then the chosen character: bars 85 pixels wide (exp 206) drawn from
// an 8 x 12 piece of their texture (stretched), "current / max" in the digit font, exp in per cent, SP, level and
// class, karma. The exp per cent is worked out even when the next level's exp equals this one's. The clip goes on
// before BeginPaint.
INT NCPawnSelectWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPawnSelectWnd::OnPaint);
    TCHAR Buf[64];
    SIZE Size;
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    Device->DrawText(15, 16, 0xFFA3A3A3, *GL2GameData.SysString[50], 0, 0, 0, L2FONT_Normal, 0,
                     0, 0, 0, 0, 0, 0);                             // "Name"
    Device->DrawText(15, 39, 0xFFA3A3A3, *GL2GameData.SysString[88], 0, 0, 0, L2FONT_Normal, 0,
                     0, 0, 0, 0, 0, 0);                             // "Lv"
    Device->DrawText(131, 69, 0xFFDCDCDC, *GL2GameData.SysString[101], 0, 0, 0, L2FONT_Normal,
                     0, 0, 0, 0, 0, 0, 0);                          // "Karma"
    if (Selected >= 0 && NumCharacters > 0)
    {
        User& Info = Characters[Selected];
        INT HPW = 0;
        INT MPW = 0;
        INT EXPW = 0;
        if (Info.MaxHP)
            HPW = Info.HP * 85 / Info.MaxHP;
        if (Info.MaxMP)
            MPW = Info.MP * 85 / Info.MaxMP;
        if (GLobbyExpTable[Info.Level + 1])
            EXPW = (INT)((DOUBLE)(Info.Exp - GLobbyExpTable[Info.Level]) * 206.0f /
                         (GLobbyExpTable[Info.Level + 1] - GLobbyExpTable[Info.Level]));

        // (a full bar is drawn 10 high where a partial one is 12, as in the stock code)
        if (HPW > 0 && HPW <= 85)
            Device->DrawTexture(35, 55, HPW, 12, 0, 0, 8, 12, HPTex);
        else if (HPW > 84)
            Device->DrawTexture(35, 55, 85, 10, 0, 0, 8, 12, HPTex);
        if (MPW > 0 && MPW <= 85)
            Device->DrawTexture(35, 69, MPW, 12, 0, 0, 8, 12, MPTex);
        else if (MPW > 84)
            Device->DrawTexture(35, 69, 85, 10, 0, 0, 8, 12, MPTex);
        if (EXPW > 0 && EXPW <= 206)
            Device->DrawTexture(35, 83, EXPW, 12, 0, 0, 8, 12, EXPTex);
        else if (EXPW > 206)
            Device->DrawTexture(35, 83, 206, 10, 0, 0, 8, 12, EXPTex);

        INT LevelExp = GLobbyExpTable[Info.Level];
        _swprintf(Buf, TEXT("%.2f%s"),
                  (DOUBLE)(Info.Exp - LevelExp) * 100.0f / (GLobbyExpTable[Info.Level + 1] - LevelExp),
                  TEXT("%"));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 138 - Size.cx / 2, 85, 0xFFDCDCDC, Buf);

        appSprintf(Buf, TEXT("%s"), TEXT("/"));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 77 - Size.cx / 2, 57, 0xFFDCDCDC, Buf);
        appSprintf(Buf, TEXT("%d"), Info.HP);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 67 - Size.cx, 57, 0xFFDCDCDC, Buf);
        appSprintf(Buf, TEXT("%d"), Info.MaxHP);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 83, 57, 0xFFDCDCDC, Buf);

        appSprintf(Buf, TEXT("%s"), TEXT("/"));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 77 - Size.cx / 2, 71, 0xFFDCDCDC, Buf);
        appSprintf(Buf, TEXT("%d"), Info.MP);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 67 - Size.cx, 71, 0xFFDCDCDC, Buf);
        appSprintf(Buf, TEXT("%d"), Info.MaxMP);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        DrawDigit(Device, 1, 83, 71, 0xFFDCDCDC, Buf);

        appSprintf(Buf, TEXT("%d"), Info.SP);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(236 - Size.cx, 56, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);

        appSprintf(Buf, TEXT("%d %s"), Info.Level, GConsole->GetClassNameString(UserInt(Info, 0x210)));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(32, 39, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);

        if (Info.Karma < 999999)
            appSprintf(Buf, TEXT("%d"), Info.Karma);
        else
            appSprintf(Buf, TEXT("%d+"), Info.Karma);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawText(236 - Size.cx, 70, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                         0, 0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009bbe0, NCPawnSelectWnd, OnPaint)

// 0x1009b670 (vslot 74) - the choice, then the combo box shows it (C4: the locked test is in OnSelect)
void NCPawnSelectWnd::SelectCharacter(INT Index)
{
    OnSelect(Index);
    if (NameCombo)
        NameCombo->SetSelected(Index);
}
NW_HOOK_METHOD(0x1009b670, NCPawnSelectWnd, SelectCharacter)

// 0x1009c590 (vslot 75) - a character being deleted asks first whether to restore it (and the lobby's Delete and Start
// buttons wait for the answer); otherwise the stage turns to it. A locked one selects nothing and leaves the buttons off. An
// index one past the last is accepted, and -1 reads the record before the first, as in the stock code. C4: the question is
// the system message 1555 (no days in it) and the Create button is left alone.
INT NCPawnSelectWnd::OnSelect(INT Index)
{
    if (NumCharacters < Index)
        Selected = -1;
    else
        Selected = Index;
    INT DaysLeft = UserDeleteDays(Characters[Selected]);
    if (DaysLeft > 0 && !UserLocked(Characters[Selected]))
    {
        GConsole->SelectedCharacterNum(-1);
        NDialogInfo Info;
        Info.ID = DIALOG_RestoreCharacter;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.Owner = Entry;
        Info.SetSystemMessage(MSG_RestoreCharacter);
        ConsoleLobbyWnd()->DialogBox->SetInfo(&Info);
        ConsoleLobbyWnd()->DialogBox->ShowWindow();
        ConsoleLobbyWnd()->FunctionWnd->DeleteButton->EnableWindow(0);
        ConsoleLobbyWnd()->StartWnd->StartButton->EnableWindow(0);
    }
    else if (DaysLeft == 0 && !UserLocked(Characters[Selected]))
    {
        GConsole->SelectedCharacterNum(Selected);
        ConsoleLobbyWnd()->FunctionWnd->DeleteButton->EnableWindow(1);
        ConsoleLobbyWnd()->StartWnd->StartButton->EnableWindow(1);
    }
    else
    {
        if (!UserLocked(Characters[Selected]))
            return 1;
        Selected = -1;
        ConsoleLobbyWnd()->FunctionWnd->DeleteButton->EnableWindow(0);
        ConsoleLobbyWnd()->StartWnd->StartButton->EnableWindow(0);
    }
    return 1;
}
NW_HOOK_METHOD(0x1009c590, NCPawnSelectWnd, OnSelect)

// 0x1009b780 (vslot 76) - the restore question answered: nothing stays selected
INT NCPawnSelectWnd::OnRestoreAnswer(INT bYes, INT Unused)
{
    if (bYes && Selected >= 0)
        GConsole->RequestCharacterRestore(Selected);
    GConsole->SelectedCharacterNum(-1);
    NameCombo->SetSelected(-1);
    Selected = -1;
    ConsoleLobbyWnd()->FunctionWnd->CreateButton->EnableWindow(1);
    ConsoleLobbyWnd()->FunctionWnd->DeleteButton->EnableWindow(1);
    ConsoleLobbyWnd()->StartWnd->StartButton->EnableWindow(1);
    return 1;
}
NW_HOOK_METHOD(0x1009b780, NCPawnSelectWnd, OnRestoreAnswer)

// 0x1009b630 (name ours) - the actor of a record: spawned by the class name of its race and sex, facing the camera,
// dressed by SetPawnResource (C2 had this inline in AddCharacter)
void NCPawnSelectWnd::SpawnCharacter(User* Info)
{
    Info->Pawn = (APawn*)SpawnActor(Info->GetClassNamePointer());
    Info->Pawn->Rotation.Yaw = 0x7FFF;
    Info->SetPawnResource();
}
NW_HOOK_METHOD(0x1009b630, NCPawnSelectWnd, SpawnCharacter)

// 0x1009c480 - one character from the server (NCLobbyWnd::AddCharacter): its actor is spawned and the engine's stage gets
// the record. The karma is copied again after the name goes into the combo box, as in the stock code. The one the server
// marked as the last used (+0x21C) is selected at once, unless it waits for its deletion (the restore question would
// come up).
INT NCPawnSelectWnd::AddCharacter(User Info)
{
    if (NumCharacters >= 7)
        return 0;
    Characters[NumCharacters] = Info;
    ComboAdd(NameCombo, Info.GetName());
    Characters[NumCharacters].Karma = Info.Karma;
    SpawnCharacter(&Characters[NumCharacters]);
    GConsole->InsertSelectCharacterInfo(NumCharacters, Characters[NumCharacters]);
    if (UserInt(Info, 0x21C))
    {
        if (UserDeleteDays(Characters[NumCharacters]) > 0 && !UserLocked(Characters[NumCharacters]))
        {
            NumCharacters++;
            return 1;
        }
        SelectCharacter(NumCharacters);
    }
    NumCharacters++;
    return 1;
}
NW_HOOK_METHOD(0x1009c480, NCPawnSelectWnd, AddCharacter)

// 0x1009b6a0 (7 bytes) - the character panel's selected slot (-1 none). Wave 3 left it stock because a stock caller (NCLobbyWnd::OnStart) kept
// ecx/edx across the call; every caller listed by xrefs is a hooked function now (regcheck judges it)
INT NCPawnSelectWnd::GetSelected()
{
    return Selected;
}
NW_HOOK_METHOD(0x1009b6a0, NCPawnSelectWnd, GetSelected)

// 0x1009b6b0
TCHAR* NCPawnSelectWnd::GetCharacterName(INT Index)
{
    return Characters[Index].GetName();
}
NW_HOOK_METHOD(0x1009b6b0, NCPawnSelectWnd, GetCharacterName)

// 0x1009b700 - the actors go, the stage forgets the characters
void NCPawnSelectWnd::ClearCharacters()
{
    for (INT i = 0; i < NumCharacters; i++)
    {
        if (Characters[i].Pawn)
        {
            Characters[i].Pawn->Destroy();
            Characters[i].Pawn = NULL;
        }
    }
    NumCharacters = 0;
    Selected = -1;
    NameCombo->ClearItems();
    GConsole->SelectedCharacterNum(-1);
    GConsole->ResetSelectCharacterInfo();
}
NW_HOOK_METHOD(0x1009b700, NCPawnSelectWnd, ClearCharacters)

NW_CHECK_VTABLE(NCPawnSelectWnd, 0x101b7d40, 77)
