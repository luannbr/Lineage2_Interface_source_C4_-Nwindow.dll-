/*=============================================================================
    StatusWnd.cpp - the messenger's status menu (C3 only; C3 addresses). Stock: 0x10130DA0 (OnMouseMove), 0x10130E10
    (KillFocus), 0x10132180 (OnPaint), 0x10134EB0 (the lines), 0x101352E0 (OnDestroy), 0x10135370 (OnLButtonUp),
    0x101365E0 (OnCreate), 0x10137860 / 0x10137900 (constructor, destructor).
=============================================================================*/
#include "nwindow/StatusWnd.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

enum
{
    STATUS_LineHeight = 17,         // the lines are 17 pixels high, from y 4 (the frame's edge is 8 wide)
    STATUS_LineCount = 8,
    STATUS_FirstLineY = 4,
    SYSSTR_StatusOnline = 1015,     // the codes' names: 1015 .. 1021, then 1001 for the last line
    SYSSTR_StatusLast = 1001,
};

// the global the messenger's buttons read before they act (0x102C3200; nothing in this image writes it)
#if NW_HOOK_MODE
#define GMsnBlocked NW_GLOBAL(INT, 0x10322C68)
#else
extern INT GMsnBlocked;                 // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

// 0x1015c1d0 - the owner and the protocol object are the arguments; no line yet, nothing under the mouse, no texture
StatusWnd::StatusWnd(MsnWnd* InOwner, Msn* InMsn)
{
    Owner = InOwner;
    MsnObj = InMsn;
    Hover = -1;
    for (INT i = 0; i < 9; i++)
        FrameTex[i] = NULL;
    SelectTex = NULL;
    SelectTex2 = NULL;
}
NW_HOOK_CTOR_ARGS(0x1015c1d0, StatusWnd, (MsnWnd* A, Msn* B), (A, B))

// 0x1015c270 - the list of lines is freed (0x1001F130)
NW_HOOK_DTOR(0x1015c270, StatusWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT StatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT StatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)   // the standalone: NControl::WindowProc's body (0x10148f10)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// 0x10155a80 (vslot 30) - nothing under the mouse any more, the menu goes, then NCWnd's
void StatusWnd::KillFocus()
{
    Hover = -1;
    HideWindow();
    NCWnd::KillFocus();
}
NW_HOOK_METHOD(0x10155a80, StatusWnd, KillFocus)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1015afd0 (vslot 37) - NCWnd's, the nine frame textures, the highlight's two, and the lines
INT StatusWnd::OnCreate()
{
    TCHAR Name[264];
    NCWnd::OnCreate();
    for (INT i = 1; i <= 9; i++)
    {
        appSprintf(Name, TEXT("L2UI_CH3.Tooltip.Tooltip%d"), i);
        FrameTex[i - 1] = FindTexture(Name);
    }
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    BuildItems();
    return 1;
}
NW_HOOK_METHOD(0x1015afd0, StatusWnd, OnCreate)

// the line of the menu (repeated inline in the stock BuildItems): the text, -1, the picture (NULL for none)
static FORCEINLINE void AddLine(TArray<NListItem>& Items, NString* Text, UTexture* Icon)
{
    INT Index = Items.Add(1);
    NListItem& Item = Items(Index);
    Item.Text = Text;
    Item.Param = -1;
    Item.Unknown8 = (INT)Icon;
}

// 0x10159a70 (name ours; anonymous in the db) - the lines, in the order of the status codes: online, busy, be right back,
// away, on the phone, out to lunch, appear offline, and last the options (system strings 1015 .. 1021 and 1001)
void StatusWnd::BuildItems()
{
    NString* Text = new NString(*GL2GameData.SysString[1015]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon1")));
    Text = new NString(*GL2GameData.SysString[1016]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon3")));
    Text = new NString(*GL2GameData.SysString[1017]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon2")));
    Text = new NString(*GL2GameData.SysString[1018]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon2")));
    Text = new NString(*GL2GameData.SysString[1019]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon3")));
    Text = new NString(*GL2GameData.SysString[1020]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon2")));
    Text = new NString(*GL2GameData.SysString[1021]);
    AddLine(Items, Text, FindTexture(TEXT("L2UI_CH3.Msn.msnicon4")));
    Text = new NString(*GL2GameData.SysString[SYSSTR_StatusLast]);
    AddLine(Items, Text, NULL);
}
NW_HOOK_METHOD(0x10159a70, StatusWnd, BuildItems)

// 0x10159ea0 (vslot 38) - the lines' texts go and the list is emptied, then NCWnd's
INT StatusWnd::OnDestroy()
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        NString* Text = Items(i).Text;
        if (Text)
            delete Text;
    }
    Items.Empty();
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10159ea0, StatusWnd, OnDestroy)

// 0x10156e10 (vslot 62) - the nine-piece frame, then every line: the highlight bar under the one under the mouse, its
// picture, and its text (the last line, the options, has a rule over it and is grey while the messenger is blocked)
INT StatusWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (FrameTex[0])
        Device->DrawTexture(0, 0, 8, 8, 0, 0, 8, 8, FrameTex[0], 0xFF, 1);
    if (FrameTex[1])
        Device->DrawStretchedTex(8, 0, (INT)(Width - 16.0f), 8, FrameTex[1], 1, 0xFF);
    if (FrameTex[2])
        Device->DrawTexture((INT)(Width - 8.0f), 0, 8, 8, 0, 0, 8, 8, FrameTex[2], 0xFF, 1);
    if (FrameTex[3])
        Device->DrawStretchedTex(0, 8, 8, (INT)(Height - 16.0f), FrameTex[3], 1, 0xFF);
    if (FrameTex[4])
        Device->DrawStretchedTex(8, 8, (INT)(Width - 16.0f), (INT)(Height - 16.0f), FrameTex[4], 1, 0xFF);
    if (FrameTex[5])
        Device->DrawStretchedTex((INT)(Width - 8.0f), 8, 8, (INT)(Height - 16.0f), FrameTex[5], 1, 0xFF);
    if (FrameTex[6])
        Device->DrawTexture(0, (INT)(Height - 8.0f), 8, 8, 0, 0, 8, 8, FrameTex[6], 0xFF, 1);
    if (FrameTex[7])
        Device->DrawStretchedTex(8, (INT)(Height - 8.0f), (INT)(Width - 16.0f), 8, FrameTex[7], 1, 0xFF);
    if (FrameTex[8])
        Device->DrawTexture((INT)(Width - 8.0f), (INT)(Height - 8.0f), 8, 8, 0, 0, 8, 8, FrameTex[8], 0xFF, 1);

    INT Y = STATUS_FirstLineY;
    for (INT Line = 0; Line < Items.Num(); Line++)
    {
        NString* Text = Items(Line).Text;
        UTexture* Icon = (UTexture*)Items(Line).Unknown8;
        if (Line == Hover)
        {
            // the highlight: its body and its right end; the last line's sits 2 below the line's top, the others' 2 above
            INT BarY = (Line == Items.Num() - 1) ? Y + 2 : Y - 2;
            Device->DrawStretchedTex(2, BarY, (INT)(Width - 24.0f - 4.0f), 0x11, 0, 0, 0x10, 0xD, SelectTex, 0xFF);
            Device->DrawStretchedTex((INT)(Width + 2.0f - 24.0f - 4.0f), BarY, 0x20, 0x11, 0, 0, 0x20, 0xD, SelectTex2, 0xFF);
        }
        if (Icon)
            Device->DrawTexture(4, Y, 0xD, 0xD, 0, 0, 0xD, 0xD, Icon, 0xFF, 1);
        if (Line == Items.Num() - 1 && Text && Text->GetAt(0))
        {
            Device->DrawLine(4, Y, (INT)(Width - 8.0f), Y, 0xFF7E7E7E, 0);
            if (GMsnBlocked)
                Device->DrawText(0x16, Y + 4, 0xFF7E7E7E, Text->GetAt(0), 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
            else
                Device->DrawText(0x16, Y + 6, 0xFFDCDCDC, Text->GetAt(0), 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Text && Text->GetAt(0))
        {
            Device->DrawText(0x16, Y + 2, 0xFFDCDCDC, Text->GetAt(0), 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
        }
        Y += STATUS_LineHeight;
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10156e10, StatusWnd, OnPaint)

// 0x10155a10 (vslot 67) - the line under the mouse (17 pixels each, from y 2) is the hot one, at the last at the most;
// the position is the message's 16-bit pair in the window's coordinates, and ignored outside the window
INT StatusWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = (WORD)(LOWORD(lParam) - (WORD)ScreenX);
    INT Y = (WORD)(HIWORD(lParam) - (WORD)ScreenY);
    if ((FLOAT)X <= Width && (FLOAT)Y <= Height)
    {
        Hover = (Y - 2) / STATUS_LineHeight;
        if (Hover >= Items.Num())
            Hover = Items.Num() - 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x10155a10, StatusWnd, OnMouseMove)

// 0x10159f30 (vslot 63, OnLButtonUp; the db says OnLButtonDown) - a click on the hot line: the first six set the status code on the protocol object ("NLN",
// "BSY", "BRB", "IDL", "PHN", "LUN"), the seventh asks the messenger window to sign out, the eighth opens the options
// (or says so when the messenger is blocked); then the menu goes, whatever was hit
INT StatusWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT Index = -1;
    INT X = (WORD)(LOWORD(lParam) - (WORD)ScreenX);
    INT Y = (WORD)(HIWORD(lParam) - (WORD)ScreenY);
    if ((FLOAT)X <= Width && (FLOAT)Y <= Height)
        Index = (Y - 2) / STATUS_LineHeight;
    if (Index == Hover && MsnObj)
    {
        const TCHAR* Code = NULL;
        switch (Index)
        {
        case 0: Code = TEXT("NLN"); break;
        case 1: Code = TEXT("BSY"); break;
        case 2: Code = TEXT("BRB"); break;
        case 3: Code = TEXT("IDL"); break;
        case 4: Code = TEXT("PHN"); break;
        case 5: Code = TEXT("LUN"); break;
        }
        if (Code)
        {
            MsnSetStatus(MsnObj, Code);                                      // the protocol object's SetStatus
            HideWindow();
            return 1;
        }
        if (Index == 6)
        {
            Owner->ShowOfflineDialog();
            HideWindow();
            return 1;
        }
        if (Index == 7)
        {
            if (GMsnBlocked)
            {
                Owner->ShowMessageDialog(*GL2GameData.SystemMsg[1309].Message);
                HideWindow();
                return 1;
            }
            Owner->ShowOptionWnd();
        }
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10159f30, StatusWnd, OnLButtonUp)

NW_CHECK_VTABLE(StatusWnd, 0x101dcd70, 74)
