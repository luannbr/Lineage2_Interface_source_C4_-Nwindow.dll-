/*=============================================================================
    NCMacroListWnd.cpp - the macro window: the grid, the counter, the buttons, and the macros it keeps.
    Stock (C3): 0x1009B050 (WindowProc), 0x1009B0D0 / 0x1009B130 (the icons' hit tests), 0x1009B190 (OnMouseMove),
    0x1009B2A0 (Minimize), 0x1009C050 .. 0x1009C170 (what the console's events do to the macro being run), 0x1009C4E0
    (SetIcon), 0x1009C550 (OnPaint), 0x1009C7E0 (OnNew), 0x1009EE40 (AddMacro), 0x1009FC60 .. 0x1009FD3F (constructor),
    0x1009FD40 (destructor), 0x1009FDC0 (OnDestroy), 0x1009FF30 (the tool tips), 0x100A0C30 (OnCreate).
    ShowWindow (0x100AA860) is NCSystemMenuWnd's, OnLButtonUp (0x1000BD60) a folded `return 1`.
=============================================================================*/
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCMacroArrayWnd.h"
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCMacroEditWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"

// 0x10032490 (NToolTip.cpp; anonymous in the db, name ours) - a tip whose text is a system string. NToolTipInfo's header has no such
// method: the stock function is written on a stand-in that is the same object
struct NToolTipInfoTextIDExt
{
    void SetTextID(INT InTextID, INT InStyle);
};

enum
{
    MSG_MacroListFull = 0x31D,         // systemmsg (name ours): no more than 24 macros
    COLOR_SystemMsg = 0xFFB09B79,
    MACRO_NumIcons = 7,                // the icons "L2UI.MacroWnd.Macro_Icon1".."7"
    MACRO_FrameID = 2,                 // the title bar's id (name ours)
};

// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12; names ours)
enum
{
    STR_Macro = 655,                  // 0x67940: the window's heading, at (11, 32)
    STR_MacroEditTip = 656,           // 0x6794C: the edit icon's tool tip
    STR_MacroDeleteTip = 892,         // 0x6845C: the delete icon's
    STR_MacroNew = 0x28E,             // the "new" button
    STR_MacroHelp = 0x91,             // the help button
};

// the command codes of NCommandParser::GetCommand the running line's wait is cut short for (/pickup; /target,
// /targetnext, /assist); the seconds it is cut to
enum { LINECMD_PickUp = 0xE, LINECMD_Target = 0xC, LINECMD_TargetNext = 0xD, LINECMD_Assist = 0xF };

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

// the slot of the shortcut bar at Index (the desktop keeps the bar at +0x138, its slots at +0x1E0; C3: twelve a page)
static FORCEINLINE NShortCutItem* ShortCutSlot(INT Index)
{
    return &GConsole->GetDesktop()->ShortCutWnd->Items[Index];
}

/*-----------------------------------------------------------------------------
    The desktop's toggle of the editor (anonymous in the db, name ours): NCConsole.h's ToggleMacroEdit is the one of
    the information window (0x10053A10, window at +0x134), this one is the editor's (+0x130).
-----------------------------------------------------------------------------*/

// 0x1005bf30 - hides the editor with the sound 6 when it shows; else shows it, gives it the focus, sound 5
void NMacroDesktop::ToggleMacroEditWindow()
{
    if (NewWndFC->IsShowWindow())
    {
        GConsole->PlayInterfaceSound(6);
        NewWndFC->HideWindow();
    }
    else
    {
        NewWndFC->ShowWindow();
        NewWndFC->SetFocus();
        GConsole->PlayInterfaceSound(5);
    }
}
NW_HOOK_METHOD(0x1005bf30, NMacroDesktop, ToggleMacroEditWindow)

/*-----------------------------------------------------------------------------
    Constructor, destructor, WindowProc.
-----------------------------------------------------------------------------*/

// 0x100ac320 - the two icons' rectangles cleared; the macros' array and the two tool tips are constructed
NCMacroListWnd::NCMacroListWnd()
{
    DeleteY = 0;
    DeleteX = 0;
    DeleteW = 0;
    DeleteH = 0;
    EditY = 0;
    EditX = 0;
    EditW = 0;
    EditH = 0;
}
NW_HOOK_CTOR(0x100ac320, NCMacroListWnd)

// 0x100aa860 (vslot 44): the stock body is NCSystemMenuWnd's (the window takes the place of its minimize icon), not NCWnd's
void NCMacroListWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (DWORD)(INT)Width, (DWORD)(INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}

// 0x100ac400 - the two tool tips and the macros' array are destroyed
NW_HOOK_DTOR(0x100ac400, NCMacroListWnd)

// 0x100a7720 (vslot 25) - the two buttons
INT NCMacroListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MACROLIST_NEW)
            return OnNew() != 0;
        if (wParam == NID_MACROLIST_HELP)
            return OnHelp() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a7720, NCMacroListWnd, WindowProc)

// vslot 69 - the macro help page (the stock body, 0x1009b300, is NCMacroEditWnd::OnHelp's, which hooks it)
INT NCMacroListWnd::OnHelp()
{
    GDesktop->ToggleHelp(1);
    return 1;
}

/*-----------------------------------------------------------------------------
    The icons' hit tests, the mouse and the macros.
-----------------------------------------------------------------------------*/

// 0x100a77a0 (ret 8; sub_ in the db) - (X, Y) inside the delete icon's rectangle (its size in floats, cut)
UBOOL NCMacroListWnd::HitDelete(INT X, INT Y)
{
    INT RelX = X - (DeleteX + ScreenX);
    INT RelY = Y - (DeleteY + ScreenY);
    if (RelX >= 0 && RelX <= (INT)DeleteW && RelY >= 0 && RelY <= (INT)DeleteH)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x100a77a0, NCMacroListWnd, HitDelete)

// 0x100a7800 (ret 8; sub_ in the db) - the same for the edit icon
UBOOL NCMacroListWnd::HitEdit(INT X, INT Y)
{
    INT RelX = X - (EditX + ScreenX);
    INT RelY = Y - (EditY + ScreenY);
    if (RelX >= 0 && RelX <= (INT)EditW && RelY >= 0 && RelY <= (INT)EditH)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x100a7800, NCMacroListWnd, HitEdit)

// 0x100a7860 (vslot 67) - the mouse over each of the two icons (their rectangles in floats: the point is compared as one)
INT NCMacroListWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = (INT)(lParam & 0xFFFF);
    INT Y = (INT)((DWORD)lParam >> 16);
    INT Left = EditX + ScreenX;
    if (Left <= X && (FLOAT)X <= (FLOAT)Left + EditW)
    {
        INT Top = EditY + ScreenY;
        if (Top <= Y && (FLOAT)Y <= (FLOAT)Top + EditH)
        {
            bOverEdit = 1;
            goto Delete;
        }
    }
    bOverEdit = 0;
Delete:
    Left = DeleteX + ScreenX;
    if (Left <= X && (FLOAT)X <= (FLOAT)Left + DeleteW)
    {
        INT Top = DeleteY + ScreenY;
        if (Top <= Y && (FLOAT)Y <= (FLOAT)Top + DeleteH)
        {
            bOverDelete = 1;
            return 1;
        }
    }
    bOverDelete = 0;
    return 1;
}
NW_HOOK_METHOD(0x100a7860, NCMacroListWnd, OnMouseMove)

// 0x100a7970 (vslot 54) - minimizing: the icon shows the macro window's picture (and is brought in when the window was
// left of the screen), the window hides and the icon takes the focus
void NCMacroListWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.MacroWnd.MacroListIcon"), 0x4A6, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100a7970, NCMacroListWnd, Minimize)

// 0x100a8730 - the console's event (NConsoleWnd slot 219): the macro being run waits for /pickup to finish: its line's
// wait is cut to one second
void NCMacroListWnd::OnPickUpDone()
{
    INT i;
    for (i = 0; i < Macros.Num(); i++)
    {
        if (Macros(i) != NULL && Macros(i)->bRunning != 0)
            break;
    }
    if (i >= Macros.Num())
        return;
    NMacroEntry* Macro = Macros(i);
    INT Offset = Macro->CurLine;
    if (Macro->Lines[Offset].Type == NMLT_Command && Macro->Parser.GetCommand(Macro->Lines[Offset].Text) == LINECMD_PickUp)
        Macros(i)->Lines[Offset].Duration = 1.0f;
}
NW_HOOK_METHOD(0x100a8730, NCMacroListWnd, OnPickUpDone)

// 0x100a87c0 - the console's event (NConsoleWnd slot 218): the same for /target, /targetnext and /assist, cut to a
// thousandth of a second
void NCMacroListWnd::OnTargetDone()
{
    INT i;
    for (i = 0; i < Macros.Num(); i++)
    {
        if (Macros(i) != NULL && Macros(i)->bRunning != 0)
            break;
    }
    if (i >= Macros.Num())
        return;
    NMacroEntry* Macro = Macros(i);
    INT Offset = Macro->CurLine;
    if (Macro->Lines[Offset].Type == NMLT_Command)
    {
        INT Command = Macro->Parser.GetCommand(Macro->Lines[Offset].Text);
        if (Command == LINECMD_Target || Command == LINECMD_TargetNext || Command == LINECMD_Assist)
            Macros(i)->Lines[Offset].Duration = 0.001f;
    }
}
NW_HOOK_METHOD(0x100a87c0, NCMacroListWnd, OnTargetDone)

// 0x100a8850 - the console's SkillCoolTime event (NConsoleWnd slot 217): skill SkillID (0: any) is cooling for TimeMS
// milliseconds. The macro being run, when its line uses that skill (as a skill line, a /useskill command or a shortcut
// of the skill), waits that long (in whole seconds, rounded up; one second when the time is 0)
void NCMacroListWnd::OnSkillCoolTime(INT SkillID, INT TimeMS)
{
    for (INT i = 0; i < Macros.Num(); i++)
    {
        if (Macros(i) != NULL && Macros(i)->bRunning != 0)
        {
            NMacroEntry* Macro = Macros(i);
            INT Offset = Macro->CurLine;
            NMacroLine* Line = &Macro->Lines[Offset];
            INT Skill;
            if (Line->Type == NMLT_Skill)
                Skill = Line->Param08;
            else if (Line->Type == NMLT_ShortCut)
            {
                INT Page = Line->Param0C;
                INT Slot = Line->Param08;
                if (Page < 0 || Page > 9 || Slot < 0 || Slot > 11)
                    return;
                Skill = AsC4((NItemEntry*)ShortCutSlot(Slot + Page * 12))->ID;
            }
            else if (Line->Type != NMLT_Command)
                return;
            else
            {
                NCommandParser Parser;
                INT Command = Parser.GetCommand(Line->Text);
                if (Command == 0x19 || Command == 0x1A)
                {
                    // /useskill <name of the skill>: the name is the tokens joined with blanks, the skill the table of the commands' skills gives
                    TCHAR Name[300];
                    appMemset(Name, 0, 600);
                    TCHAR* Token = Parser.GetNextToken();
                    Parser.ExpandToken(Token);
                    appSprintf(Name, TEXT("%s"), Token);
                    Token = Parser.GetNextToken();
                    Parser.ExpandToken(Token);
                    while (Token != NULL)
                    {
                        appStrcat(Name, TEXT(" "));
                        appStrcat(Name, Token);
                        Token = Parser.GetNextToken();
                        Parser.ExpandToken(Token);
                    }
                    Skill = *(INT*)GL2GameData.GetMSCommandData(Name);
                }
                else if (Command == 0x1C || Command == 0x1D)
                {
                    // /useshortcut <page> <slot>: the skill of that shortcut
                    TCHAR* Token = Parser.GetNextToken();
                    Parser.ExpandToken(Token);
                    if (Token == NULL)
                        return;
                    INT Page = NWtol(Token);
                    if (Page < 1 || Page > 10)
                        return;
                    Token = Parser.GetNextToken();
                    Parser.ExpandToken(Token);
                    if (Token == NULL)
                        return;
                    INT Slot = NWtol(Token);
                    if (Slot < 1 || Slot > 12)
                        return;
                    NItemEntryC4* Item = AsC4((NItemEntry*)ShortCutSlot(Slot + Page * 12 - 13));
                    if (Item->Unknown1910 == SHORTCUT_Skill)   // (the shortcut's kind: C4 +0x1B18)
                        Skill = Item->ID;
                    else
                        Skill = (INT)(INT_PTR)this;   // the stock code leaves the register holding this: never a skill (kept as is)
                }
                else
                    Skill = (INT)(INT_PTR)this;       // the same: no skill known for any other command
            }
            if (SkillID != 0 && Skill != SkillID)
                return;
            if (TimeMS % 1000 > 0)
                Macros(i)->Lines[Offset].Duration = (FLOAT)(TimeMS / 1000 + 1);
            else if (TimeMS != 0)
                Macros(i)->Lines[Offset].Duration = (FLOAT)(TimeMS / 1000 + 1);
            else
                Macros(i)->Lines[Offset].Duration = 1.0f;
            return;
        }
    }
}
NW_HOOK_METHOD(0x100a8850, NCMacroListWnd, OnSkillCoolTime)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100a8bc0 - the icon texture of macro Index from its icon number
void NCMacroListWnd::SetIcon(INT Index)
{
    if (Index < Macros.Num() && Index >= 0)
    {
        NMacroEntry* Macro = Macros(Index);
        INT Type = Macro->IconType;
        if (Type >= 0 && Type < MACRO_NumIcons)
        {
            TCHAR Name[50];
            appSprintf(Name, TEXT("%s%d"), TEXT("L2UI.MacroWnd.Macro_Icon"), Type + 1);
            Macro->IconTex = FindTexture(Name);
        }
    }
}
NW_HOOK_METHOD(0x100a8bc0, NCMacroListWnd, SetIcon)

// 0x100ab4b0 - a macro added to the list: its tool tip is built (name and description) and its icon found
void NCMacroListWnd::AddMacro(NMacroEntry* Macro)
{
    if (Macro != NULL)
    {
        Macro->ToolTip.FillMacro(&Macro->ToolTip, NTT_Text);
        Macros.AddItem(Macro);
        SetIcon(Macros.Num() - 1);
    }
}
NW_HOOK_METHOD(0x100ab4b0, NCMacroListWnd, AddMacro)

/*-----------------------------------------------------------------------------
    The window.
-----------------------------------------------------------------------------*/

// 0x100ad360 (vslot 37) - the background, the New (id 0x100F, text 0x28E, at 11, 347) and Help (0x100C, text 0x91, at
// 11, 372) buttons, the icons' textures (edit, delete, each with its dragged one), their rectangles (34 x 34 at (169, 347)
// and (208, 347)) and tool tips, the title bar and the grid (6 columns, 80 macros' room, 32 pixel cells); the slots'
// texts cleared
INT NCMacroListWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("l2ui_ch3.macrownd.macro2_back"));

    NewButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    NewButton->ID = NID_MACROLIST_NEW;
    NewButton->SetPosition(11, 347, 76, 23);
    CreateChild(NewButton, NWS_VISIBLE);
    NewButton->SetTextID(STR_MacroNew);

    HelpButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    HelpButton->ID = NID_MACROLIST_HELP;
    HelpButton->SetPosition(11, 372, 76, 23);
    CreateChild(HelpButton, NWS_VISIBLE);
    HelpButton->SetTextID(STR_MacroHelp);

    EditTex = FindTexture(TEXT("L2UI_CH3.macrownd.macro_edit"));
    EditDragTex = FindTexture(TEXT("L2UI_CH3.macrownd.macro_edit_Drag"));
    DeleteTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.Inventory_trash"));
    DeleteDragTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.Inventory_trash_Drag"));
    bEditDragOver = 0;
    bDeleteDragOver = 0;
    bOverEdit = 0;
    bOverDelete = 0;
    EditW = 34.0f;
    EditH = 34.0f;
    EditX = 0xA9;
    EditY = 0x15B;
    ((NToolTipInfoTextIDExt*)&EditToolTip)->SetTextID((INT)STR_MacroEditTip, 0x400);   // C4: NToolTipInfo's text by system string id (Clear, id, flags, Layout)
    DeleteW = 34.0f;
    DeleteH = 34.0f;
    DeleteX = 0xD0;
    DeleteY = 0x15B;
    ((NToolTipInfoTextIDExt*)&DeleteToolTip)->SetTextID((INT)STR_MacroDeleteTip, 0x400);

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = MACRO_FrameID;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    Array = new NCMacroArrayWnd(6, 8, 0x50, 0x20);
    Array->SetPosition(6, 45, 244, 285);
    CreateChild(Array, NWS_VISIBLE | 0x20);

    for (INT i = 0; i < NMACRO_MaxMacros; i++)
    {
        NMacroInfo* Info = &Slots[i].Info;
        appMemset(Info->ShortName, 0, sizeof(Info->ShortName));
        appMemset(Info->Name, 0, sizeof(Info->Name));
        appMemset(Info->Description, 0, sizeof(Info->Description));
        for (INT j = 0; j < NMACRO_MaxLines; j++)
            appMemset(Info->Lines[j].Text, 0, sizeof(Info->Lines[j].Text));
    }
    Selected = 0;
    return 1;
}
NW_HOOK_METHOD(0x100ad360, NCMacroListWnd, OnCreate)

// 0x100ac480 (vslot 38) - the macros are destroyed and their memory freed; the buttons, the title bar and the grid go
INT NCMacroListWnd::OnDestroy()
{
    guard(NCMacroListWnd::OnDestroy);
    for (INT i = 0; i < Macros.Num(); i++)
    {
        NMacroEntry* Macro = Macros(i);
        if (Macro != NULL)
        {
            Macro->~NMacroEntry();   // 0x100ac100
            appFree(Macro);
        }
    }
    Macros.Empty();
    DestroyChild(this, NewButton);
    DestroyChild(this, HelpButton);
    DestroyChild(this, FrameCtrl);
    DestroyChild(this, Array);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ac480, NCMacroListWnd, OnDestroy)

// 0x100a8c30 (vslot 56) - the background, the two icons (the dragged texture while the button is down on it), the
// counter "(n/24)" at the right and the heading
INT NCMacroListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMacroListWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawStretchedTex(0, 20, 256, 381, 0, 0, 256, 381, BackTex);
    if (EditTex != NULL && bEditDragOver == 0)
        Device->DrawTexture(EditX, EditY, (INT)EditW, (INT)EditH, 0, 0, 34, 34, EditTex, 0xFF, 1);
    else if (EditDragTex != NULL && bEditDragOver != 0)
        Device->DrawTexture(EditX, EditY, (INT)EditW, (INT)EditH, 0, 0, 34, 34, EditDragTex, 0xFF, 1);
    if (DeleteTex != NULL && bDeleteDragOver == 0)
        Device->DrawTexture(DeleteX, DeleteY, (INT)DeleteW, (INT)DeleteH, 0, 0, 34, 34, DeleteTex, 0xFF, 1);
    else if (DeleteDragTex != NULL && bDeleteDragOver != 0)
        Device->DrawTexture(DeleteX, DeleteY, (INT)DeleteW, (INT)DeleteH, 0, 0, 34, 34, DeleteDragTex, 0xFF, 1);
    TCHAR Count[14];
    appSprintf(Count, Macros.Num() < 10 ? TEXT("(0%d/%d)") : TEXT("(%d/%d)"), Macros.Num(), NMACRO_MaxMacros);
    Device->DrawText(200, 32, 0xFFDCDCDC, Count, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_Macro), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a8c30, NCMacroListWnd, OnPaint)

// 0x100ac5f0 (vslot 57; the db's OnPostPaint) - the tool tip of the icon the mouse is over, drawn by the window's skin
// at the icon's corner
INT NCMacroListWnd::OnDrawToolTip(NCGDevice* Device)
{
    guard(NCMacroListWnd::OnPostPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bOverEdit != 0 && ToolTipSkin != NULL)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, EditToolTip, EditX, EditY, Wnd);
    }
    if (bOverDelete != 0 && ToolTipSkin != NULL)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, DeleteToolTip, DeleteX, DeleteY, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ac5f0, NCMacroListWnd, OnDrawToolTip)

// 0x100a8ec0 (vslot 74) - New: the editor opens with empty boxes for the next slot (message 0x31D when all 24 are used)
INT NCMacroListWnd::OnNew()
{
    if (GConsole != NULL && GConsole->Desktop != NULL)
    {
        NCMacroEditWnd* Edit = (NCMacroEditWnd*)GConsole->GetDesktop()->NewWndFC;
        if (Edit != NULL && Edit->EditList != NULL)
        {
            NCMacroEditList* EditList = Edit->EditList;
            if (Edit->NameEdit != NULL)
                Edit->NameEdit->ClearText();
            Selected = Macros.Num();
            if (Macros.Num() >= NMACRO_MaxMacros)
            {
                GConsole->ShowSystemMessage(MSG_MacroListFull, COLOR_SystemMsg);
                return 1;
            }
            for (INT i = 0; i < NMACRO_EditLines; i++)
            {
                if (EditList->Edits(i) != NULL)
                    EditList->Edits(i)->ClearText();
            }
            ToggleMacroEditWindow(GConsole->GetDesktop());
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100a8ec0, NCMacroListWnd, OnNew)

NW_CHECK_VTABLE(NCMacroListWnd, 0x101ba378, 76)
