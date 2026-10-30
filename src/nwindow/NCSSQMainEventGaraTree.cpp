/*=============================================================================
    NCSSQMainEventGaraTree.cpp - the Seven Signs event list's tree. C3 stock: 0x100EA260 (OnCreate), 0x100EA370
    (paint), 0x100EA460 .. 0x100EAF7F (DrawItem), 0x100EB5A0 / 0x100EB5F0 (constructor, destructor).
=============================================================================*/
#include "nwindow/NCSSQMainEventGaraTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NSSQHelpers.h"

// 0x100fdf40 - NCGaraTree's alone
NCSSQMainEventGaraTree::NCSSQMainEventGaraTree()
{
}
NW_HOOK_CTOR(0x100fdf40, NCSSQMainEventGaraTree)

// 0x100fdf90
NW_HOOK_DTOR(0x100fdf90, NCSSQMainEventGaraTree)

// 0x10041760 (vslot 25) - the stock body is the one the linker shared with NCExButton: NControl's, as a flag
INT NCSSQMainEventGaraTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100fcb40 (vslot 37) - NControl's, then the three panels, the line and the two halves of the selection bar
INT NCSSQMainEventGaraTree::OnCreate()
{
    NControl::OnCreate();
    Panel1Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL1"));
    Panel2Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL2"));
    Panel3Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL3"));
    LineTex = FindTexture(TEXT("L2UI_CH3.Etc.TextBackline"));
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    return 1;
}
NW_HOOK_METHOD(0x100fcb40, NCSSQMainEventGaraTree, OnCreate)

// 0x100fcc60 (vslot 62) - NCGaraTree paint, the clip 304 pixels high and the entries starting at (15, 5)
INT NCSSQMainEventGaraTree::OnPaint(NCGDevice* Device)
{
    guard(NCSSQMainEventGaraTree::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY + ScrollOffset, (INT)Width, 304);
    ContentHeight = DrawItem(Device, Root, 15, 5);
    SetWindowSize(Width, (FLOAT)ContentHeight);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fcc60, NCSSQMainEventGaraTree, OnPaint)

// 0x100fcd50 (vslot 72) - the entry is put at (X, Y) (its place is kept for the others) and drawn by its kind:
//   1 a heading (the event group): its picture, a bar behind the text when it is open and the group's name (the
//     text of sysstring 845 for the group 0);
//   2 an event: its picture, the event's name, the count of what was won (or "(a b)": the player's count against
//     the needed one) and the two numbers under it;
//   3 the event's details: the panel (3 parts) on the banded background, the cabals' two lists of names.
// The children are laid out under it (3 pixels in for the kind 0, half the picture's width in for the others) and
// hidden when the entry is closed (but not for the kind 0). The height used comes back
INT NCSSQMainEventGaraTree::DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y)
{
    if (!Item)
        return 0;
    NGaraTVItemData& Data = Item->Data;
    TCHAR LabelText[128];
    TCHAR CountText[512];
    TCHAR NameText[512];
    INT Used = 0;
    Item->CopyW = X;
    Item->CopyH = Y;
    if (Item->Children.Num() > 0)
        Item->SetPosition(X, Y, Data.Width + 16, Data.Height);
    else
        Item->SetPosition(X, Y, Data.Width, Data.Height);
    if (Data.Kind == 1)
    {
        if (!Item->bExpanded)
        {
            Device->DrawTexture(X, Y, Data.IconW, Data.IconH, 0, 0, Data.IconW, Data.IconH, Data.Icon, 0xFF, 1);
        }
        else
        {
            Device->DrawTexture(X, Y, Data.IconW, Data.IconH, 0, 0, Data.IconW, Data.IconH, Data.ExpandedIcon, 0xFF,
                                1);
            Device->DrawStretchedTex(Data.IconW + X, Y + 1, (INT)(Width - 47.0f - 2.0f), 13, 0, 0, 16, 13, SelectTex,
                                     0xFF);
            Device->DrawStretchedTex((INT)((FLOAT)(Data.IconW + X) + Width - 47.0f - 2.0f), Y + 1, 32, 13, 0, 0, 32, 13,
                                     SelectTex2, 0xFF);
        }
        if (Data.QuestID == 0)
            Device->DrawText(Data.IconW + X + 5, Y + 2, 0xFFDCDCDC, *GL2GameData.SysString[845], (INT)Width, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        SIZE CharSize;
        NCGDevice::GetCharSize(&CharSize, 'A', 0);
        Used = CharSize.cy + 10;
    }
    else if (Data.Kind == 2)
    {
        Device->DrawTexture(X, Y, Data.IconW, Data.IconH, 0, 0, Data.IconW, Data.IconH,
                            Item->bExpanded ? Data.ExpandedIcon : Data.Icon, 0xFF, 1);
        appStrcpy(NameText, SSQEventName(GConsole, NULL, Data.QuestID));
        Device->DrawText(Data.IconW + X + 5, Y + 2, 0xFF808080, NameText, (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);
        SIZE NameSize;
        NCGDevice::GetTextExtent(&NameSize, NameText, 0, 0, 0, 0, 0);
        if (Data.bTextCount)
        {
            if (Data.IconSet > Data.LevelMin)
                appSprintf(CountText, TEXT("(%s %s)"), *GL2GameData.SysString[923], *GL2GameData.SysString[828]);
            else if (Data.IconSet < Data.LevelMin)
                appSprintf(CountText, TEXT("(%s %s)"), *GL2GameData.SysString[924], *GL2GameData.SysString[828]);
            else
                appSprintf(CountText, TEXT("(%s)"), *GL2GameData.SysString[846]);
            Device->DrawText(Data.IconW + NameSize.cx + X + 10, Y + 2, 0xFFB09B79, CountText, (INT)Width, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            Device->DrawText(Data.IconW + NameSize.cx + X + 10, Y + 2, 0xFFB09B79, *GL2GameData.SysString[829],
                             (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        appSprintf(LabelText, TEXT("%s : "), *GL2GameData.SysString[831]);
        Device->DrawText(Data.IconW + X + 5, NameSize.cy + Y + 6, 0xFF808080, LabelText, (INT)Width, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        SIZE LabelSize;
        NCGDevice::GetTextExtent(&LabelSize, LabelText, 0, 0, 0, 0, 0);
        Device->DrawText(Data.IconW + LabelSize.cx + X + 5, NameSize.cy + Y + 6, 0xFFB09B79, appItoa(Data.LevelMax),
                         (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Used = NameSize.cy * 2 + 14;
    }
    else if (Data.Kind == 3)
    {
        SIZE CharSize;
        NCGDevice::GetCharSize(&CharSize, 'A', 0);
        INT NumA = 0;
        INT NumB = 0;
        for (INT i = 0; i < 9; i++)
        {
            if (Data.Strings1DD8[i].Len() > 0)
                NumA++;
            if (Data.Strings1E44[i].Len() > 0)
                NumB++;
        }
        Used = (NumB + NumA + 7) * (CharSize.cy + 4);
        Device->DrawTexture(X - 5, Y - 3, 0xD3, 8, 0, 0, 0xD3, 8, Panel1Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, Y + 5, 0xD3, Used - 16, 0, 0, 0xD3, 8, Panel2Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, Used + Y - 11, 0xD3, 8, 0, 0, 0xD3, 8, Panel3Tex, 0xFF, 1);
        INT NumBands = Used / 15;
        INT BandY = Y - 2;
        for (INT i = 0; i < NumBands; i++)
        {
            if (i % 2 == 1)
                Device->DrawTexture(X - 5, BandY, 0xD3, 15, 0, 0, 8, 15, LineTex, 0xFF, 1);
            BandY += 16;
        }
        Device->DrawText(X, Y, 0xFF808080, SSQCabalName(GConsole, NULL, 2), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                         0);
        SIZE HeaderSize;
        NCGDevice::GetTextExtent(&HeaderSize, *GL2GameData.SysString[830], 0, 0, 0, 0, 0);
        Device->DrawText(X, HeaderSize.cy + Y + 4, 0xFF808080, *GL2GameData.SysString[830], 0, 0, 0, L2FONT_Normal,
                         0, 0, 0, 0, 0, 0, 0);
        appSprintf(NameText, TEXT("%d"), Data.IconSet);
        Device->DrawText(HeaderSize.cx + X + 4, CharSize.cy + Y + 4, 0xFFB09B79, NameText, 0, 0, 0, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0, 0);
        Device->DrawText(X, Y + CharSize.cy * 2 + 8, 0xFF808080, *GL2GameData.SysString[832], 0, 0, 0, L2FONT_Normal,
                         0, 0, 0, 0, 0, 0, 0);
        for (INT j = 0; j < NumA; j++)
            Device->DrawText(X, (j + 3) * (CharSize.cy + 4) + Y, 0xFFB09B79, *Data.Strings1DD8[j], 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(X, (CharSize.cy + 4) * (NumA + 4) + Y, 0xFF808080, SSQCabalName(GConsole, NULL, 1), 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(X, (CharSize.cy + 4) * (NumA + 5) + Y, 0xFF808080, *GL2GameData.SysString[830], 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        appSprintf(CountText, TEXT("%d"), Data.LevelMin);
        Device->DrawText(HeaderSize.cx + X + 4, (CharSize.cy + 4) * (NumA + 5) + Y, 0xFFB09B79, CountText, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(X, (CharSize.cy + 4) * (NumA + 6) + Y, 0xFF808080, *GL2GameData.SysString[832], 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        for (INT k = 0; k < NumB; k++)
            Device->DrawText(X, (CharSize.cy + 4) * (NumA + 7 + k) + Y, 0xFFB09B79, *Data.Strings1E44[k], 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    if (Item->Children.Num() > 0)
    {
        if (!Item->bExpanded && Data.Kind != 0)
        {
            for (INT i = 0; i < Item->Children.Num(); i++)
                Item->Children(i)->HideWindow();
            return Used;
        }
        for (INT i = 0; i < Item->Children.Num(); i++)
        {
            if (Data.Kind == 0)
                Used += DrawItem(Device, Item->Children(i), 3, Used + Y);
            else
                Used += DrawItem(Device, Item->Children(i), Data.IconW / 2 + X, Used + Y);
        }
    }
    return Used;
}
NW_HOOK_METHOD(0x100fcd50, NCSSQMainEventGaraTree, DrawItem)

NW_CHECK_VTABLE(NCSSQMainEventGaraTree, 0x101c9d48, 79)
