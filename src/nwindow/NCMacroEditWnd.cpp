/*=============================================================================
    NCMacroEditWnd.cpp - the macro editor.
    Stock (C3): 0x1009B300 (OnHelp), 0x1009B480 .. 0x1009B6EA (WindowProc, the icon arrows, the information window's
    toggle, HideWindow, Minimize), 0x1009DBB0 (OnPaint), 0x1009DE70 .. 0x1009E2CF (the marks, the lines for the
    request), 0x1009E2D0 (ShowWindow), 0x1009E480 (OnDestroy), 0x1009F260 (OnSave), 0x100A0310 .. 0x100A0BFF
    (constructor, destructor, OnCreate). OnCancel (0x1007BEE0) is the body NCOptionWnd::OnCancel hooks.
=============================================================================*/
#include "nwindow/NCMacroEditWnd.h"
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCMacroInfoWnd.h"
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"

enum
{
    MSG_MacroLinesInvalid = 0x32A,     // systemmsg (names ours): a line is not a command, or there is a gap
    MSG_MacroNameEmpty = 0x346,
    MSG_MacroNameUsed = 0x347,
    COLOR_SystemMsg = 0xFFB09B79,
    MACRO_NumIcons = 7,                // the icons to choose from
    MACRO_NameLength = 12,             // the name box's longest text
    MACRO_ShortNameLength = 4,         // the short name box's
    MACRO_DescLength = 32,             // the description's (the information window keeps it)
};

// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12; names ours)
enum
{
    STR_MacroName = 656,              // 0x6794C: the name box's label at (11, 32), light
    STR_MacroIconLabel = 50,          // 0x65CE4: drawn at (65, 63), grey
    STR_MacroShortNameLabel = 653,    // 0x67928: drawn at (65, 86), grey
    STR_Cancel = 141,
    STR_MacroHelp = 145,              // the button 0x100C's text (0x91)
    STR_MacroSave = 658,              // 0x292
    STR_MacroInfo = 657,              // 0x291
};

// the desktop's macro windows (NCConsole.h keeps them as NCWnd*: NewWndF8 the list, NewWndFC this, NewWnd100 the
// information window; names ours)
static NCMacroListWnd* DesktopList()
{
    return (NCMacroListWnd*)GConsole->GetDesktop()->NewWndF8;
}
static NCMacroInfoWnd* DesktopInfo()
{
    return (NCMacroInfoWnd*)GConsole->GetDesktop()->NewWnd100;
}

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

/*-----------------------------------------------------------------------------
    Constructor, destructor, WindowProc and the small handlers.
-----------------------------------------------------------------------------*/

// 0x100ac9e0 - the icon textures' array and the three buttons the window keeps by hand are cleared
NCMacroEditWnd::NCMacroEditWnd()
{
    SaveButton = NULL;
    HelpButton = NULL;
    CancelButton = NULL;
}
NW_HOOK_CTOR(0x100ac9e0, NCMacroEditWnd)

// 0x100aca50 - the textures' array is destroyed (the stock calls the item entry arrays' destructor, 0x1001d7a0)
NW_HOOK_DTOR(0x100aca50, NCMacroEditWnd)

// 0x100a7b30 (vslot 25) - the buttons
INT NCMacroEditWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MACROEDIT_SAVE)
            return OnSave() != 0;
        if (wParam == NID_MACROEDIT_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_MACROEDIT_PREVICON)
            return OnPrevIcon() != 0;
        if (wParam == NID_MACROEDIT_NEXTICON)
            return OnNextIcon() != 0;
        if (wParam == NID_MACROEDIT_INFO)
            return OnInfo() != 0;
        if (wParam == NID_MACROEDIT_HELP)
            return OnHelp() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a7b30, NCMacroEditWnd, WindowProc)

// 0x100a7c20 (vslot 77) - the icon before (round the seven)
INT NCMacroEditWnd::OnPrevIcon()
{
    IconIndex--;
    if (IconIndex < 0)
        IconIndex = MACRO_NumIcons - 1;
    return 1;
}
NW_HOOK_METHOD(0x100a7c20, NCMacroEditWnd, OnPrevIcon)

// 0x100a7c40 (vslot 78) - the icon after
INT NCMacroEditWnd::OnNextIcon()
{
    IconIndex++;
    if (IconIndex > MACRO_NumIcons - 1)
        IconIndex = 0;
    return 1;
}
NW_HOOK_METHOD(0x100a7c40, NCMacroEditWnd, OnNextIcon)

// 0x100a7ca0 (vslot 79) - the additional information window: hidden when it shows, else shown with its sound (0x10053A10,
// the desktop's toggle of the window at +0x134, which NCConsole.h calls ToggleMacroEdit)
INT NCMacroEditWnd::OnInfo()
{
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopInfo() != NULL)
    {
        if (DesktopInfo()->IsShowWindow())
            GConsole->GetDesktop()->NewWnd100->HideWindow();
        else
            GConsole->GetDesktop()->ToggleMacroEdit();
    }
    return 1;
}
NW_HOOK_METHOD(0x100a7ca0, NCMacroEditWnd, OnInfo)

// 0x100a7d00 (vslot 76) - the macro help page
INT NCMacroEditWnd::OnHelp()
{
    GDesktop->ToggleHelp(1);
    return 1;
}
NW_HOOK_METHOD(0x100a7d00, NCMacroEditWnd, OnHelp)

// vslot 69 - HideWindow (the stock body, 0x1007bee0, is NCOptionWnd::OnCancel's, which hooks it)
INT NCMacroEditWnd::OnCancel()
{
    HideWindow();
    return 1;
}

// 0x100a7c70 (ret 4; the db had it as NCMacroInfoWnd's) - the short name box emptied, then Text put in it
void NCMacroEditWnd::SetShortName(const TCHAR* Text)
{
    ShortNameEdit->ClearText();
    if (Text != NULL && ShortNameEdit != NULL)
        ShortNameEdit->InsertText(Text, 1);
}
NW_HOOK_METHOD(0x100a7c70, NCMacroEditWnd, SetShortName)

// 0x100a7d20 (vslot 49) - the information window goes with the editor
void NCMacroEditWnd::HideWindow()
{
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopInfo() != NULL)
        DesktopInfo()->HideWindow();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100a7d20, NCMacroEditWnd, HideWindow)

// 0x100a7d60 (vslot 54) - minimizing: the icon shows the editor's picture (and is brought in when the window was left of
// the screen), the window hides and the icon takes the focus
void NCMacroEditWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.MacroWnd.MacroEditIcon"), 0x290, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100a7d60, NCMacroEditWnd, Minimize)

// 0x100aa520 - the mark of every line box cleared (NCEditBox +0x1D8, the line highlight)
void NCMacroEditWnd::ClearMarks()
{
    for (INT i = 0; i < NMACRO_EditLines; i++)
        EditList->Edits(i)->Unknown1D8 = 0;
}
NW_HOOK_METHOD(0x100aa520, NCMacroEditWnd, ClearMarks)

// 0x100aa5d0 - the mark of one line box cleared
void NCMacroEditWnd::ClearMark(INT Line)
{
    if (Line < NMACRO_EditLines)
        EditList->Edits(Line)->Unknown1D8 = 0;
}
NW_HOOK_METHOD(0x100aa5d0, NCMacroEditWnd, ClearMark)

/*-----------------------------------------------------------------------------
    The window.
-----------------------------------------------------------------------------*/

// 0x100acab0 (vslot 37) - the seven icons, Help / Save / Cancel buttons (Help 0x100C at x 11, Save 0x1011 at 91, Cancel
// 0x1018 at 171, all at y 372), the arrows and the information button, the name and short name boxes (tab order chained
// between them and the window), the title bar and the lines
INT NCMacroEditWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("l2ui_ch3.macrownd.macro1_back"));
    IconTexs.Empty();
    for (INT i = 1; i <= MACRO_NumIcons; i++)
    {
        TCHAR Name[50];
        appSprintf(Name, TEXT("%s%d"), TEXT("L2UI.MacroWnd.Macro_Icon"), i);
        IconTexs.AddItem(FindTexture(Name));
    }
    IconIndex = 0;

    HelpButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    HelpButton->ID = NID_MACROEDIT_HELP;
    HelpButton->SetPosition(11, 372, 76, 23);
    CreateChild(HelpButton, NWS_VISIBLE);
    HelpButton->SetTextID(0x91);

    SaveButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    SaveButton->ID = NID_MACROEDIT_SAVE;
    SaveButton->SetPosition(91, 372, 76, 23);
    CreateChild(SaveButton, NWS_VISIBLE);
    SaveButton->SetTextID(0x292);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_MACROEDIT_CANCEL;
    CancelButton->SetPosition(171, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(0x8D);

    PrevIconButton = new NCButton(TEXT("L2UI_ch3.Button.Prev1"), TEXT("L2UI_ch3.Button.Prev1_Down"), 0);
    PrevIconButton->ID = NID_MACROEDIT_PREVICON;
    PrevIconButton->SetPosition(16, 93, 15, 15);
    CreateChild(PrevIconButton, NWS_VISIBLE);

    NextIconButton = new NCButton(TEXT("L2UI_ch3.Button.next1"), TEXT("L2UI_ch3.Button.next1_down"), 0);
    NextIconButton->ID = NID_MACROEDIT_NEXTICON;
    NextIconButton->SetPosition(36, 93, 15, 15);
    CreateChild(NextIconButton, NWS_VISIBLE);

    InfoButton = new NCButton(TEXT("L2UI_ch3.button.SmallButton2"), TEXT("L2UI_ch3.button.SmallButton2_Down"), 0);
    InfoButton->ID = NID_MACROEDIT_INFO;
    InfoButton->SetPosition(177, 82, 66, 21);
    CreateChild(InfoButton, NWS_VISIBLE);
    InfoButton->SetTextID(0x291);

    NameEdit = new NCEditBox(0, 0, 1, 0x108, 0);
    NameEdit->ID = NID_MACROEDIT_NAME;
    NameEdit->SetMaxLength(MACRO_NameLength);
    NameEdit->SetPosition(93, 60, 148, 17);
    CreateChild(NameEdit, NWS_VISIBLE);

    ShortNameEdit = new NCEditBox(0, 0, 1, 0, 0);
    ShortNameEdit->SetMaxLength(MACRO_ShortNameLength);
    ShortNameEdit->ID = NID_MACROEDIT_SHORTNAME;
    ShortNameEdit->SetPosition(117, 83, 54, 17);
    CreateChild(ShortNameEdit, NWS_VISIBLE);

    SetTabOrder(NameEdit->Entry, ShortNameEdit->Entry);
    NameEdit->SetTabOrder(ShortNameEdit->Entry, Entry);
    ShortNameEdit->SetTabOrder(Entry, NameEdit->Entry);

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = 1;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    EditList = new NCMacroEditList(9, 12);
    EditList->SetPosition(7, 125, 242, 239);
    CreateChild(EditList, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100acab0, NCMacroEditWnd, OnCreate)

// 0x100aaaf0 (vslot 38)
INT NCMacroEditWnd::OnDestroy()
{
    guard(NCMacroEditWnd::OnDestroy);
    DestroyChild(this, FrameCtrl);
    DestroyChild(this, HelpButton);
    DestroyChild(this, SaveButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, PrevIconButton);
    DestroyChild(this, NextIconButton);
    DestroyChild(this, InfoButton);
    DestroyChild(this, NameEdit);
    DestroyChild(this, ShortNameEdit);
    DestroyChild(this, EditList);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100aaaf0, NCMacroEditWnd, OnDestroy)

// 0x100aa260 (vslot 56) - the background, the icon chosen, the labels of the boxes and the macro's short name (up to
// four characters, two to a line) next to the icon
INT NCMacroEditWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMacroEditWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopList() != NULL)
    {
        if (BackTex)
            Device->DrawStretchedTex(0, 20, 256, 381, 0, 0, 256, 381, BackTex);
        Device->DrawTexture(17, 55, 32, 32, 0, 0, 32, 32, IconTexs(IconIndex));
        Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_MacroName), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(65, 63, 0xFFA3A3A3, *SysString(STR_MacroIconLabel), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(65, 86, 0xFFA3A3A3, *SysString(STR_MacroShortNameLabel), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        if (ShortNameEdit != NULL)
        {
            const TCHAR* ShortName = ShortNameEdit->CommitText();
            if (ShortName != NULL)
            {
                INT X = 21;
                INT Y = 59;
                FString Text(ShortName);
                for (INT k = 0; k < Text.Len(); k++)
                {
                    Device->DrawText(X, Y, 0xFFDCDCDC, *Text.Mid(k, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                    if ((k & 1) == 0)
                        X += 13;
                    else
                    {
                        Y += 13;
                        X -= 13;
                    }
                }
            }
        }
        Device->ReleaseClip();
        Device->EndPaint();
        return 1;
    }
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x100aa260, NCMacroEditWnd, OnPaint)

// 0x100aa930 (vslot 44) - the window takes the place of its minimize icon (the icon hides); with the list and the
// information window there, the short name and the description of the slot being edited are put in the boxes (the
// description also kept in the information window), the lines are laid out from the first one, the first line takes the
// focus, the icon is the one of the macro being edited and the information window is hidden
void NCMacroEditWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon != NULL)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (DWORD)(INT)Width, (DWORD)(INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
    if (GConsole == NULL || GConsole->Desktop == NULL)
        return;
    NCMacroListWnd* List = DesktopList();
    NCMacroInfoWnd* Info = DesktopInfo();
    if (List == NULL || Info == NULL)
        return;
    NMacroInfo& SlotInfo = List->Slots[List->Selected].Info;
    TCHAR* ShortName = SlotInfo.ShortName;
    if (ShortName == NULL)
        SetShortName(NULL);
    else if (appStrlen(ShortName) <= MACRO_ShortNameLength)
        SetShortName(ShortName);
    TCHAR* Desc = SlotInfo.Description;
    if (Desc == NULL)
    {
        Info->SetDescription(NULL);
        appStrcpy(Info->Description, Desc);
    }
    else if (appStrlen(Desc) <= MACRO_DescLength)
    {
        Info->SetDescription(Desc);
        appStrcpy(Info->Description, Desc);
    }
    EditList->LayoutLines(0);
    EditList->Edits(0)->SetFocus();
    EditList->UpdateSlider();
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopList() != NULL)
        List = DesktopList();
    if (List->Selected < List->Macros.Num())
        IconIndex = List->Macros(List->Selected)->IconType;
    DesktopInfo()->HideWindow();
}
NW_HOOK_METHOD(0x100aa930, NCMacroEditWnd, ShowWindow)

/*-----------------------------------------------------------------------------
    Saving.
-----------------------------------------------------------------------------*/

// 0x100ab8d0 (vslot 68) - Save: the lines must be valid (message 0x32A), the name given (0x346) and not another macro's
// (0x347, compared without case); then the window closes (the information window's description and the short name
// box's text are put in the slot: C3) and the macro goes to the server: its id (0 for a new one), the name (12
// characters at most), the description and the short name of the slot, the icon and the lines
INT NCMacroEditWnd::OnSave()
{
    if (GConsole == NULL || GConsole->Desktop == NULL || DesktopList() == NULL)
        return 0;
    NCMacroListWnd* List = DesktopList();
    if (!EditList->ValidateLines())
    {
        GConsole->ShowSystemMessage(MSG_MacroLinesInvalid, COLOR_SystemMsg);
        return 0;
    }
    if (NameEdit->CommitText() == NULL)
    {
        GConsole->ShowSystemMessage(MSG_MacroNameEmpty, COLOR_SystemMsg);
        return 0;
    }
    INT Num = List->Macros.Num();
    INT Sel = List->Selected;
    INT i;
    for (i = 0; i < Num; i++)
    {
        if (Sel < Num)
        {
            if (List->Macros(Sel)->ID != List->Macros(i)->ID && List->Macros(i)->GetName() != NULL)
            {
                if (appStricmp(List->Macros(i)->GetName(), NameEdit->CommitText()) == 0)
                    break;
            }
        }
        else if (List->Macros(i)->GetName() != NULL)
        {
            if (appStricmp(List->Macros(i)->GetName(), NameEdit->CommitText()) == 0)
                break;
        }
        Num = List->Macros.Num();
    }
    if (i < List->Macros.Num())
    {
        GConsole->ShowSystemMessage(MSG_MacroNameUsed, COLOR_SystemMsg);
        return 0;
    }
    HideWindow();
    Sel = List->Selected;
    NMacroSlot* Slot = &List->Slots[Sel];
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopInfo() != NULL)
    {
        TCHAR* Desc = DesktopInfo()->Description;
        if (Desc == NULL)
            appMemset(Slot->Info.Description, 0, 0x42);
        else if (appStrlen(Desc) <= MACRO_DescLength)
        {
            appMemset(Slot->Info.Description, 0, 0x42);
            appStrcpy(Slot->Info.Description, Desc);
        }
        if (ShortNameEdit->CommitText() == NULL)
            appMemset(Slot->Info.ShortName, 0, 10);
        else if (appStrlen(ShortNameEdit->CommitText()) <= MACRO_ShortNameLength)
        {
            appMemset(Slot->Info.ShortName, 0, 10);
            appStrcpy(Slot->Info.ShortName, ShortNameEdit->CommitText());
        }
    }
    if (Sel < 0 || Sel >= NMACRO_MaxMacros)
        return 1;
    L2ParamStack Params(0x44);
    if (Sel < List->Macros.Num())
        Params.PushBack((void*)(INT_PTR)List->Macros(Sel)->ID);
    else if (Sel == List->Macros.Num())
        Params.PushBack((void*)0);
    if (NameEdit->CommitText() != NULL && appStrlen(NameEdit->CommitText()) <= MACRO_NameLength)
        Params.PushBack((void*)NameEdit->CommitText());
    else
        Params.PushBack((void*)0);
    Params.PushBack((void*)Slot->Info.Description);
    Params.PushBack((void*)Slot->Info.ShortName);
    Params.PushBack((void*)(INT_PTR)IconIndex);
    PushLines(&Params);
    GConsole->RequestMakeMacro(Params);
    return 1;
}
NW_HOOK_METHOD(0x100ab8d0, NCMacroEditWnd, OnSave)

// the command code of the lines the stock parser recognizes (NCommandParser::GetCommand); names ours
enum
{
    LINECMD_UseSkill = 0x18,           // /useskill <name>: the macro keeps the skill's id
    LINECMD_UseShortCut = 0x1B,        // /useshortcut <page> <slot>
    LINECMD_Delay = 0x4C,              // /delay <seconds>
};

// 0x100aa600 - the lines for the save request: how many of the boxes hold text (the first empty one's number, 12 when
// none is), then for each of the 12 lines a record of five values: its number (from 1), its type (1 skill, 3 text,
// 4 shortcut, 6 delay), two numbers (the skill's id and whether the name was found; the page and the slot, from 0; the
// delay) and the text (only for the type 3). A line the parser rejects is left out. C4: every command code below 0x6E is
// sent as plain text, the empty lines included (their text is NULL), except /useskill, /useshortcut and /delay; the codes
// 0x10000000 / 0x10000001 only for a builder. The numbers advance even for a /delay or /useshortcut that turns out invalid.
void NCMacroEditWnd::PushLines(L2ParamStack* Params)
{
    if (Params == NULL)
        return;
    INT Count = 0;
    do
    {
        if (EditList->Edits(Count)->CommitText() == NULL)
        {
            Params->PushBack((void*)(INT_PTR)Count);
            break;
        }
        Count++;
    }
    while (Count < NMACRO_EditLines);
    if (Count == NMACRO_EditLines)
        Params->PushBack((void*)NMACRO_EditLines);

    INT Number = 0;
    for (INT i = 0; i < NMACRO_EditLines; i++)
    {
        const TCHAR* Text = EditList->Edits(i)->CommitText();
        TCHAR Line[34];
        INT Command;
        if (Text == NULL)
        {
            appMemset(Line, 0, 0x42);   // 33 characters of the 34 (as in the stock code)
            Command = -1;
        }
        else
        {
            appStrcpy(Line, Text);
            Command = EditList->Parser.GetCommand(Line);
        }
        UBOOL bPlain = 0;
        if (Command > LINECMD_Delay)
        {
            if (Command >= 0x10000000)
            {
                if (Command <= 0x10000001)
                {
                    User* Player = GConsole->GetMyUser();
                    if (Player != NULL && Player->BuilderLevel != 0)
                        bPlain = 1;
                }
                else if (Command == 0x20000000)
                    bPlain = 1;
            }
            else if (Command < 0x6E)
                bPlain = 1;
        }
        else if (Command == LINECMD_Delay)
        {
            Number++;
            TCHAR* Token = EditList->Parser.GetNextToken();
            INT Seconds;
            if (Token != NULL && (Seconds = NWtol(Token)) >= 1)
            {
                Params->PushBack((void*)(INT_PTR)Number);
                Params->PushBack((void*)6);
                Params->PushBack((void*)(INT_PTR)Seconds);
                Params->PushBack((void*)0);
                Params->PushBack((void*)0);
            }
        }
        else if ((UINT)(Command - LINECMD_UseSkill) <= 3)
        {
            if (Command == LINECMD_UseSkill)
            {
                TCHAR SkillName[300];
                appSprintf(SkillName, TEXT("%s"), EditList->Parser.GetNextToken());
                TCHAR* Token = EditList->Parser.GetNextToken();
                while (Token != NULL)
                {
                    appSprintf(SkillName, TEXT("%s %s"), SkillName, Token);
                    Token = EditList->Parser.GetNextToken();
                }
                Number++;
                FL2MagicSkillCommandData* Data = GL2GameData.GetMSCommandData(SkillName);
                INT SkillID = 0;
                INT bFound = 0;
                if (Data != NULL)
                {
                    SkillID = *(INT*)Data;
                    bFound = 1;
                }
                Params->PushBack((void*)(INT_PTR)Number);
                Params->PushBack((void*)1);
                Params->PushBack((void*)(INT_PTR)SkillID);
                Params->PushBack((void*)(INT_PTR)bFound);
                Params->PushBack((void*)0);
            }
            else if (Command == LINECMD_UseShortCut)
            {
                Number++;
                TCHAR* Token = EditList->Parser.GetNextToken();
                INT Page;
                INT Slot;
                if (Token != NULL && (Page = NWtol(Token)) >= 1 && Page <= 10
                    && (Token = EditList->Parser.GetNextToken()) != NULL && (Slot = NWtol(Token)) >= 1 && Slot <= 12)
                {
                    Params->PushBack((void*)(INT_PTR)Number);
                    Params->PushBack((void*)4);
                    Params->PushBack((void*)(INT_PTR)(Page - 1));
                    Params->PushBack((void*)(INT_PTR)(Slot - 1));
                    Params->PushBack((void*)0);
                }
            }
            else
                bPlain = 1;
        }
        else if (Command < 0x6E)
            bPlain = 1;
        if (bPlain)
        {
            Number++;
            Params->PushBack((void*)(INT_PTR)Number);
            Params->PushBack((void*)3);
            Params->PushBack((void*)0);
            Params->PushBack((void*)0);
            Params->PushBack((void*)Text);
        }
    }
}
NW_HOOK_METHOD(0x100aa600, NCMacroEditWnd, PushLines)

NW_CHECK_VTABLE(NCMacroEditWnd, 0x101ba798, 80)
