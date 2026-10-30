/*=============================================================================
    NCMacroArrayWnd.cpp - the grid of the macros of NCMacroListWnd.
    Stock (C3): 0x1009B320 / 0x1009B360 (OnCreate, OnDestroy), 0x1009C8A0 .. 0x1009D090 (the cell under a point, OnPaint,
    the mouse), 0x1009D090 (the delete's answer), 0x1009D1B0 .. 0x1009D6EF (OnLButtonUp), 0x1009D6F0 (UnselectAll),
    0x1009FAF0 (the tool tip), 0x100A00A0 .. 0x100A0140 (constructors, destructor).
    WindowProc (0x100DC990) is NCQuestItemWnd's body, which other classes keep the address of.
=============================================================================*/
#include "nwindow/NCMacroArrayWnd.h"
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCMacroEditWnd.h"
#include "nwindow/NCMacroInfoWnd.h"
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NClasses.gen.h"

enum
{
    MSG_DeleteMacro = 0x33C,           // systemmsg (name ours): "Do you want to delete <macro>?"
    MACRO_CellOrigin = 4,              // px from the grid corner to the first cell (C3)
    MACRO_CellPitchX = 5,              // px between two cells of a row, added to CellSize
    MACRO_CellPitchY = 3,              // the same between two rows
    SOUND_MacroReady = 9,              // the interface sound played when the cool time of a macro ends (name ours)
};

// the names of the commands the lines of a macro are written with (GL2GameData table of command names at +0x6E72C,
// indexed by the C2 command codes of NCommandParser::GetCommand + 1; names ours)
enum
{
    CMD_UseSkill = 0x19,
    CMD_UseShortCut = 0x1C,
    CMD_Delay = 0x4D,
};

// the "/" every command starts with (a pointer to it at 0x102186C4; name ours)
#if NW_HOOK_MODE
#define GCommandSlash NW_GLOBAL(const TCHAR*, 0x10255A9C)
#else
extern const TCHAR* GCommandSlash;
#endif

// the macro editor of the desktop (NCConsole.h keeps it as an NCWnd*: NewWndFC; name ours)
static NCMacroEditWnd* DesktopEdit()
{
    return (NCMacroEditWnd*)GConsole->GetDesktop()->NewWndFC;
}

/*-----------------------------------------------------------------------------
    Constructors and destructor.
-----------------------------------------------------------------------------*/

// 0x100edaf0 (vslot 25, ret 0xC): the stock body is NCQuestItemWnd's (hooked there): NCItemWnd's, then only the destroy dialog's answer.
// Hook build: the slot jumps to the stock address, which our hook owns (vt_slots proves a stub, not a longer body written on the class);
// the standalone has the body
#if NW_HOOK_MODE
NW_NAKED INT NCMacroArrayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x100edaf0)
#else
INT NCMacroArrayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCItemWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0xF0000003)   // NWM_DIALOG_RESULT
        return OnDialogResult(wParam, lParam) != 0;
    return 0;
}
#endif

// 0x100ac770 - the default constructor (NCItemWnd's, then the vtable)
NCMacroArrayWnd::NCMacroArrayWnd()
{
}
NW_HOOK_CTOR(0x100ac770, NCMacroArrayWnd)

// 0x100ac810 (ret 0x10; sub_ in the db) - the item window with the cell gaps 1 and 1
NCMacroArrayWnd::NCMacroArrayWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, 1, 1)
{
}
NW_HOOK_CTOR_ARGS(0x100ac810, NCMacroArrayWnd, (INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize), (InColumns, InVisibleRows, InMaxItems, InCellSize))

// 0x100ac7c0
NW_HOOK_DTOR(0x100ac7c0, NCMacroArrayWnd)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100a79d0 (vslot 37) - the two outlines (C3)
INT NCMacroArrayWnd::OnCreate()
{
    NCItemWnd::OnCreate();
    OutlineDownTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine_down"));
    OutlineTex = FindTexture(TEXT("l2ui_ch3.InventoryWnd.Inventory_OutLine"));
    return 1;
}
NW_HOOK_METHOD(0x100a79d0, NCMacroArrayWnd, OnCreate)

// 0x100a7a10 (vslot 38) - guarded; the item window's and nothing else
INT NCMacroArrayWnd::OnDestroy()
{
    guard(NCMacroArrayWnd::OnDestroy);
    NCItemWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a7a10, NCMacroArrayWnd, OnDestroy)

/*-----------------------------------------------------------------------------
    The cells.
-----------------------------------------------------------------------------*/

// 0x100a8f80 (ret 8) - the cell under (X, Y): the grid is 6 cells wide and 7 high (42), 37 pixels apart across and 35
// down from 4 pixels in, each 32 square (the rectangle is made of floats)
INT NCMacroArrayWnd::HitCell(INT X, INT Y)
{
    NRectF Cell;
    Cell.W = 32.0f;
    Cell.H = 32.0f;
    for (INT i = 0; i < 42; i++)
    {
        INT CellX = ScreenX + MACRO_CellOrigin;
        INT CellY = ScreenY + MACRO_CellOrigin;
        switch (i)
        {
            case 0: break;
            case 1: CellX += 0x25; break;
            case 2: CellX += 0x4A; break;
            case 3: CellX += 0x6F; break;
            case 4: CellX += 0x94; break;
            case 5: CellX += 0xB9; break;
            case 6: CellY += 0x23; break;
            case 7: CellX += 0x25; CellY += 0x23; break;
            case 8: CellX += 0x4A; CellY += 0x23; break;
            case 9: CellX += 0x6F; CellY += 0x23; break;
            case 10: CellX += 0x94; CellY += 0x23; break;
            case 11: CellX += 0xB9; CellY += 0x23; break;
            case 12: CellY += 0x46; break;
            case 13: CellX += 0x25; CellY += 0x46; break;
            case 14: CellX += 0x4A; CellY += 0x46; break;
            case 15: CellX += 0x6F; CellY += 0x46; break;
            case 16: CellX += 0x94; CellY += 0x46; break;
            case 17: CellX += 0xB9; CellY += 0x46; break;
            case 18: CellY += 0x69; break;
            case 19: CellX += 0x25; CellY += 0x69; break;
            case 20: CellX += 0x4A; CellY += 0x69; break;
            case 21: CellX += 0x6F; CellY += 0x69; break;
            case 22: CellX += 0x94; CellY += 0x69; break;
            case 23: CellX += 0xB9; CellY += 0x69; break;
            case 24: CellY += 0x8C; break;
            case 25: CellX += 0x25; CellY += 0x8C; break;
            case 26: CellX += 0x4A; CellY += 0x8C; break;
            case 27: CellX += 0x6F; CellY += 0x8C; break;
            case 28: CellX += 0x94; CellY += 0x8C; break;
            case 29: CellX += 0xB9; CellY += 0x8C; break;
            case 30: CellY += 0xAF; break;
            case 31: CellX += 0x25; CellY += 0xAF; break;
            case 32: CellX += 0x4A; CellY += 0xAF; break;
            case 33: CellX += 0x6F; CellY += 0xAF; break;
            case 34: CellX += 0x94; CellY += 0xAF; break;
            case 35: CellX += 0xB9; CellY += 0xAF; break;
            case 36: CellY += 0xD2; break;
            case 37: CellX += 0x25; CellY += 0xD2; break;
            case 38: CellX += 0x4A; CellY += 0xD2; break;
            case 39: CellX += 0x6F; CellY += 0xD2; break;
            case 40: CellX += 0x94; CellY += 0xD2; break;
            case 41: CellX += 0xB9; CellY += 0xD2; break;
        }
        Cell.X = CellX;
        Cell.Y = CellY;
        if (PtInRect(X, Y, Cell))
            return i;
    }
    return -1;
}
NW_HOOK_METHOD(0x100a8f80, NCMacroArrayWnd, HitCell)

// 0x100a9230 (vslot 56) - each macro icon in its cell (as many rows as fit) with its outline, its short name over it, two
// characters to a line, the outline of the selected one, and the cool time: the frame of the cool time over it while it
// runs, the flash brightened when it ends (with the sound)
INT NCMacroArrayWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMacroArrayWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    INT X = MACRO_CellOrigin;
    INT Y = MACRO_CellOrigin;
    for (INT i = 0; i < List->Macros.Num(); i++)
    {
        NMacroEntry* Macro = List->Macros(i);
        if (Macro != NULL)
        {
            if (i % Columns == 0)
            {
                X = MACRO_CellOrigin;
                if (i != TopRow * Columns)
                    Y += MACRO_CellPitchY + CellSize;
                INT Limit = (CellSize + 1) * VisibleRows;
                if (Limit - Y == 0 || Limit < Y)
                    break;
            }
            else
                X += MACRO_CellPitchX + CellSize;
            if (Macro->GetIconTex() != NULL)
            {
                Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Macro->GetIconTex(), 0xFF, 1);
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, OutlineTex, 0xFF, 1);
            }
            const TCHAR* ShortName = Macro->GetShortName();
            if (ShortName != NULL)
            {
                INT TextX = X + 4;
                INT TextY = Y + 4;
                FString Text(ShortName);
                for (INT k = 0; k < Text.Len(); k++)
                {
                    Device->DrawText(TextX, TextY, 0xFFDCDCDC, *Text.Mid(k, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                    if ((k & 1) == 0)
                        TextX += 13;
                    else
                    {
                        TextY += 13;
                        TextX -= 13;
                    }
                }
            }
            if (Macro->IsSelected())
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, OutlineDownTex, 0xFF, 1);
            if (Macro->CoolTime.bFlashing != 0)
            {
                Device->DrawBrightenTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize,
                                            Macro->CoolTime.GetFlashTexture(), 0xFF008080);
                if (Macro->CoolTime.bSoundDue != 0)
                {
                    GConsole->PlayInterfaceSound(SOUND_MacroReady);
                    Macro->CoolTime.bSoundDue = 0;
                }
            }
            if (Macro->CoolTime.bRunning != 0 && !Macro->CoolTime.IsIdle() && Macro->CoolTime.GetTexture() != NULL)
                Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Macro->CoolTime.GetTexture(), 0xFF, 1);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a9230, NCMacroArrayWnd, OnPaint)

// 0x100ac1a0 (vslot 57, named OnPostPaint in the database) - the tool tip of the macro under the mouse: the full box, at
// the position of the cell
INT NCMacroArrayWnd::OnDrawToolTip(NCGDevice* Device)
{
    guard(NCMacroArrayWnd::OnPostPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    if (List == NULL)
        return 0;
    if (HoverIndex >= 0 && HoverIndex < List->Macros.Num())
    {
        NMacroEntry* Macro = List->Macros(HoverIndex);
        if (Macro == NULL)
            return 0;
        INT Index = HoverIndex - TopRow * Columns;
        INT Y = (CellSize + 1) * (Index / Columns);
        INT X = (CellSize + 1) * (Index % Columns);
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            ToolTipSkin->DrawFull(Device, Macro->ToolTip, X, Y, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ac1a0, NCMacroArrayWnd, OnDrawToolTip)

/*-----------------------------------------------------------------------------
    The mouse.
-----------------------------------------------------------------------------*/

// 0x100a95c0 (vslot 68) - the cell under the mouse is selected (the other macros are not) and picked up
INT NCMacroArrayWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (Entry->Parent == NULL || Entry->Parent->Wnd == NULL)
        return 0;
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    INT Cell = HitCell(LOWORD(lParam), HIWORD(lParam));
    Selected = Cell;
    HoverIndex = Cell;
    if (Cell >= 0 && Cell < List->Macros.Num())
    {
        UnselectAll();
        List->Macros(Selected)->Select();
        bDragging = 1;
        return 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x100a95c0, NCMacroArrayWnd, OnLButtonDown)

// 0x100a9650 (vslot 67) - while the selected macro is carried the cursor is its icon, and the edit and delete icons of the
// list window light up (their dragged texture) when it is over them
INT NCMacroArrayWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    HoverIndex = HitCell(X, Y);
    if (Entry->Parent == NULL || Entry->Parent->Wnd == NULL)
        return 0;
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    if (Selected < 0 || Selected >= List->Macros.Num())
    {
        bDragging = 0;
        return 1;
    }
    if (!IsFocused())
        return 1;
    if (!bDragging)
        return 1;
    if (Selected >= 0 && Selected < List->Macros.Num())
    {
        SetCursor(4, List->Macros(Selected)->GetIconTex());
        LockCursor();
    }
    NWndEntry* Own = Entry;
    if (Own != NULL && Own->Parent != NULL && ((NCMacroListWnd*)Own->Parent->Wnd)->HitEdit(X, Y))
        ((NCMacroListWnd*)Own->Parent->Wnd)->bEditDragOver = 1;
    else
        ((NCMacroListWnd*)Own->Parent->Wnd)->bEditDragOver = 0;
    List->bDeleteDragOver = List->HitDelete(X, Y) != 0;
    return 1;
}
NW_HOOK_METHOD(0x100a9650, NCMacroArrayWnd, OnMouseMove)

// 0x100a9de0 (vslot 79) - no macro selected
void NCMacroArrayWnd::UnselectAll()
{
    if (Entry->Parent != NULL && Entry->Parent->Wnd != NULL)
    {
        NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
        for (INT i = 0; i < List->Macros.Num(); i++)
            List->Macros(i)->Deselect();
    }
}
NW_HOOK_METHOD(0x100a9de0, NCMacroArrayWnd, UnselectAll)

// 0x100a98a0 (vslot 63) - the carried macro dropped: on its own cell it runs; on the delete icon of the list window the
// question "delete?" comes up (the answer is OnDialogResult); on the edit icon the editor opens with the lines of the macro
// written back as the commands they stand for (the information window hides); on a shortcut slot the macro goes there
INT NCMacroArrayWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (bDragging == 0)
        return 0;
    bDragging = 0;
    ((NCMacroListWnd*)Entry->Parent->Wnd)->bEditDragOver = 0;
    if (Entry->Parent == NULL || Entry->Parent->Wnd == NULL)
        return 0;
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    List->bDeleteDragOver = 0;
    UnselectAll();
    INT Cell = HitCell(X, Y);
    INT Sel = Selected;
    if (Cell == Sel)
    {
        NMacroEntry* Macro = List->Macros(Sel);
        if (Macro != NULL)
            Macro->bRunning = 1;
    }
    else if (List->HitDelete(X, Y))
    {
        if (Sel >= 0 && Sel < List->Macros.Num())
        {
            L2ParamStack Unused(10);
            NDialogInfo Info;
            Info.Owner = Entry;
            Info.ID = DIALOG_MACRO_DELETE;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetSystemMessage(MSG_DeleteMacro);
            Info.SetButtons(List->Macros(Selected)->Name, NULL);
            GConsole->ShowDialog(&Info);
        }
    }
    else if (List->HitEdit(X, Y))
    {
        if (GConsole != NULL && GConsole->Desktop != NULL)
        {
            NCMacroEditWnd* Edit = DesktopEdit();
            if (Edit != NULL)
            {
                NCMacroEditList* EditList = Edit->EditList;
                NCWnd* Info = GConsole->GetDesktop()->NewWnd100;
                if (EditList != NULL && Info != NULL)
                {
                    for (INT i = 0; i < NMACRO_EditLines; i++)
                    {
                        if (EditList->Edits(i) != NULL)
                            EditList->Edits(i)->ClearText();
                    }
                    List->Selected = Selected;
                    Info->HideWindow();
                    Edit->HideWindow();
                    if (Edit->NameEdit != NULL)
                    {
                        Edit->NameEdit->ClearText();
                        Edit->NameEdit->InsertText(List->Macros(Selected)->Name, 1);
                    }
                    NMacroEntry* Macro = List->Macros(Selected);
                    if (Macro != NULL && Macro->NumLines > 0)
                    {
                        for (INT i = 0; i < List->Macros(Selected)->NumLines; i++)
                        {
                            TCHAR Text[3072];
                            appMemset(Text, 0, sizeof(Text));
                            if (EditList->Edits(i) != NULL)
                            {
                                NMacroLine* Line = &List->Macros(Selected)->Lines[i];
                                switch (Line->Type)
                                {
                                    case NMLT_Skill:
                                        appSprintf(Text, TEXT("%s%s %s"), GCommandSlash,
                                                   *GL2GameData.Unknown54188[CMD_UseSkill],
                                                   GConsole->GetSkillName(Line->Param08, Line->Param10));
                                        break;
                                    case NMLT_Action:
                                        appSprintf(Text, TEXT("%s%s"), GCommandSlash,
                                                   GConsole->GetActionName(Line->Param08));
                                        break;
                                    case NMLT_ShortCut:
                                        appSprintf(Text, TEXT("%s%s %d %d"), GCommandSlash,
                                                   *GL2GameData.Unknown54188[CMD_UseShortCut], Line->Param0C + 1,
                                                   Line->Param08 + 1);
                                        break;
                                    case NMLT_Command:
                                        appSprintf(Text, TEXT("%s"), Line->Text);
                                        break;
                                    case NMLT_Delay:
                                        appSprintf(Text, TEXT("%s%s %d"), GCommandSlash,
                                                   *GL2GameData.Unknown54188[CMD_Delay], Line->Param0C);
                                        break;
                                }
                                EditList->Edits(i)->InsertText(Text, 1);
                            }
                        }
                    }
                    ToggleMacroEditWindow(GConsole->GetDesktop());
                }
            }
        }
    }
    else
    {
        NWndEntry* Found = FindWindowAt(X, Y);
        if (Found != NULL && Found->Wnd != NULL && Found->Wnd->IsA(NCShortCutWnd::StaticClass()))
        {
            INT Slot = ((NCShortCutWnd*)Found->Wnd)->SlotAtPoint(X, Y);
            if (Slot < 0)
                return 1;
            L2ParamStack Params(10);
            Params.PushBack((void*)4);                                              // a macro
            Params.PushBack((void*)(INT_PTR)Slot);
            Params.PushBack((void*)(INT_PTR)List->Macros(Selected)->ID);
            Params.PushBack((void*)1);
            GConsole->RequestShortCutReg(Params);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100a98a0, NCMacroArrayWnd, OnLButtonUp)

// 0x100a9780 (vslot 81) - the answer to the question: the selected macro is deleted on the server; when the editor is
// open on it, it closes
INT NCMacroArrayWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    L2ParamStack Params(10);
    if (Entry->Parent == NULL || Entry->Parent->Wnd == NULL)
        return 0;
    NCMacroListWnd* List = (NCMacroListWnd*)Entry->Parent->Wnd;
    Params.PushBack((void*)(INT_PTR)List->Macros(Selected)->ID);
    // C4 stock 0x100A9811 calls 0x10068040 (NetHandler +0x268, the engine header's RequestDeleteMacro; the db name is the C3 one,
    // RequestMagicSkillUse). It called 0x10068020 (+0x264, RequestMakeMacro: the macro editor's save) with the one parameter
    GConsole->RequestMagicSkillUse(Params);
    if (GConsole != NULL && GConsole->Desktop != NULL)
    {
        NCMacroEditWnd* Edit = DesktopEdit();
        NCMacroListWnd* DesktopList = (NCMacroListWnd*)GConsole->GetDesktop()->NewWndF8;
        if (Edit != NULL && DesktopList != NULL && Edit->IsShowWindow())
        {
            if (DesktopList->Selected == Selected)
                DesktopEdit()->HideWindow();
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100a9780, NCMacroArrayWnd, OnDialogResult)

NW_CHECK_VTABLE(NCMacroArrayWnd, 0x101ba518, 82)
