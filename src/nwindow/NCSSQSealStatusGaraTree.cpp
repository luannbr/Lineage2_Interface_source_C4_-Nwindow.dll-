/*=============================================================================
    NCSSQSealStatusGaraTree.cpp - the Seven Signs seal status list tree. C3 stock: 0x100EB9F0 (OnCreate),
    0x100EBB80 (paint), 0x100EBC70 .. 0x100EC90F (DrawItem), 0x100ECFF0 / 0x100ED040 (constructor, destructor).
=============================================================================*/
#include "nwindow/NCSSQSealStatusGaraTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NSSQHelpers.h"

// 0x100ff890 - NCGaraTree's alone
NCSSQSealStatusGaraTree::NCSSQSealStatusGaraTree()
{
}
NW_HOOK_CTOR(0x100ff890, NCSSQSealStatusGaraTree)

// 0x100ff8e0
NW_HOOK_DTOR(0x100ff8e0, NCSSQSealStatusGaraTree)

// 0x10041760 (vslot 25) - the stock body is the one the linker shared with NCExButton: NControl's, as a flag
INT NCSSQSealStatusGaraTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100fe3c0 (vslot 37) - NControl's, then the three panels, the line, the selection bar and the seven bar pieces
INT NCSSQSealStatusGaraTree::OnCreate()
{
    NControl::OnCreate();
    Panel1Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL1"));
    Panel2Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL2"));
    Panel3Tex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QUESTWNDPANEL3"));
    LineTex = FindTexture(TEXT("L2UI_CH3.Etc.TextBackline"));
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
    Bar21Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar21"));
    Bar22Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar22"));
    Bar2BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar2back"));
    Bar11Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar11"));
    Bar12Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar12"));
    Bar1BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar1back"));
    BarLineTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_barline"));
    return 1;
}
NW_HOOK_METHOD(0x100fe3c0, NCSSQSealStatusGaraTree, OnCreate)

// 0x100fe570 (vslot 62) - the same paint as the event tree (304 pixels, entries at (15, 5))
INT NCSSQSealStatusGaraTree::OnPaint(NCGDevice* Device)
{
    guard(NCSSQSealStatusGaraTree::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY + ScrollOffset, (INT)Width, 304);
    ContentHeight = DrawItem(Device, Root, 15, 5);
    SetWindowSize(Width, (FLOAT)ContentHeight);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fe570, NCSSQSealStatusGaraTree, OnPaint)

// the bar of a cabal's share (0x100EBC70's two groups): the back (140 pixels at x 80), the share in the first part's
// texture and the rest of the total in the second's, or the whole total in the first's when the share is not under it;
// a one pixel line at the share
static FORCEINLINE void DrawShareBar(NCGDevice* Device, INT Y, INT Share, INT Total, UTexture* BackTex,
                                     UTexture* FillTex, UTexture* RestTex, UTexture* LineTex)
{
    Device->DrawTexture(0x50, Y, 0x8C, 0xB, 0, 0, 8, 0xB, BackTex, 0xFF, 1);
    if (Total > Share)
    {
        FLOAT Fill = (FLOAT)Share * 0.01f * 140.0f;
        Device->DrawTexture(0x50, Y, (INT)Fill, 0xB, 0, 0, 8, 0xB, FillTex, 0xFF, 1);
        Device->DrawTexture((INT)(Fill + 80.0f), Y, (INT)((FLOAT)(Total - Share) * 0.01f * 140.0f), 0xB, 0, 0, 8, 0xB,
                            RestTex, 0xFF, 1);
    }
    else
    {
        Device->DrawTexture(0x50, Y, (INT)((FLOAT)Total * 0.01f * 140.0f), 0xB, 0, 0, 8, 0xB, FillTex, 0xFF, 1);
    }
    Device->DrawTexture((INT)((FLOAT)Share * 0.01f * 140.0f + 80.0f), Y, 1, 0xB, 0, 0, 1, 0xB, LineTex, 0xFF, 1);
}

// 0x100fe660 (vslot 72) - the entry is put at (X, Y) and drawn by its kind:
//   1 a seal: its picture (and the bar behind the name when open), the seal's name, the winner's label and name and
//     the two cabals' bars with their percent (the second cabal's, then the first's) on the 140 pixel backs;
//   2 the seal's details: the three part panel on the banded background, the seal's state and the label / value
//     pairs of sysstrings 941 .. 956 (the seal 1) or 957 .. 968 (the seal 2).
// The children are laid out under it (2 pixels in for the kind 0, at X for the others) and hidden when the entry is
// closed (but not for the kind 0). The height used comes back
INT NCSSQSealStatusGaraTree::DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y)
{
    if (!Item)
        return 0;
    NGaraTVItemData& Data = Item->Data;
    TCHAR PercentText[256];
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
        INT Seal = Data.QuestID;
        INT Owner = Data.IconSet;
        INT Low = Data.LevelMin;
        INT High = Data.LevelMax;
        INT Total1 = Data.Unknown14;
        INT Total2 = Data.Unknown18;
        SIZE TitleSize;
        NCGDevice::GetTextExtent(&TitleSize, *GL2GameData.SysString[823], 0, 0, 0, 0, 0);
        Device->DrawText(X + 0x12, Y + 2, 0xFFDCDCDC, SSQSealName(GConsole, NULL, Seal), (INT)Width, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        INT RowOffset = TitleSize.cy + 7;
        INT Row = RowOffset + Y;
        Device->DrawText(X, Row, 0xFF808080, *GL2GameData.SysString[823], (INT)Width, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0, 0);
        Device->DrawText(TitleSize.cx + X + 4, Row, 0xFFB09B79, SSQCabalName(GConsole, NULL, Owner), (INT)Width, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Row += TitleSize.cy + 7;
        RowOffset += TitleSize.cy + 7;
        Device->DrawText(X, Row, 0xFF808080, SSQCabalName(GConsole, NULL, 2), (INT)Width, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0, 0);
        INT Share = (Owner == 2) ? Low : High;
        DrawShareBar(Device, Row, Share, Total1, Bar2BackTex, Bar21Tex, Bar22Tex, BarLineTex);
        appSprintf(PercentText, TEXT("%d%s"), Total1, TEXT("%"));
        SIZE PercentSize;
        NCGDevice::GetTextExtent(&PercentSize, PercentText, 0, 0, 0, 0, 0);
        Device->DrawText((INT)(150.0f - (FLOAT)(PercentSize.cx / 2)), Row + 1, 0xFFDCDCDC, PercentText, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Row += TitleSize.cy + 7;
        RowOffset += TitleSize.cy + 7;
        Device->DrawText(X, Row, 0xFF808080, SSQCabalName(GConsole, NULL, 1), (INT)Width, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0, 0);
        Share = (Owner == 1) ? Low : High;
        DrawShareBar(Device, Row, Share, Total2, Bar1BackTex, Bar11Tex, Bar12Tex, BarLineTex);
        appSprintf(PercentText, TEXT("%d%s"), Total2, TEXT("%"));
        NCGDevice::GetTextExtent(&PercentSize, PercentText, 0, 0, 0, 0, 0);
        Device->DrawText((INT)(150.0f - (FLOAT)(PercentSize.cx / 2)), Row, 0xFFDCDCDC, PercentText, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Used = RowOffset + 0x19;
    }
    else if (Data.Kind == 2)
    {
        INT Seal = Data.QuestID;
        SIZE StateSize;
        NCGDevice::GetTextExtent(&StateSize, SSQSealStateName(GConsole, NULL, Seal), (INT)Width, 0, 1, 0, 0,
                                L2FONT_Normal);
        SIZE CharSize;
        NCGDevice::GetCharSize(&CharSize, 'A', 0);
        Used = StateSize.cy + 0x18;
        if (Seal == 1)
            Used += (CharSize.cy + 6) * 16;
        else if (Seal == 2)
            Used += (CharSize.cy * 3 + 0x12) * 4;
        Device->DrawTexture(X - 5, Y - 3, (INT)(Width + 14.0f), 8, 0, 0, (INT)Width, 8, Panel1Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, Y + 5, (INT)(Width + 14.0f), Used - 0x10, 0, 0, (INT)Width, 8, Panel2Tex, 0xFF, 1);
        Device->DrawTexture(X - 5, Used + Y - 11, (INT)(Width + 14.0f), 8, 0, 0, (INT)Width, 8, Panel3Tex, 0xFF, 1);
        INT NumBands = Used / 17;
        INT BandY = Y - 2;
        for (INT i = 0; i < NumBands; i++)
        {
            if (i % 2 == 1)
                Device->DrawTexture(X - 5, BandY, (INT)Width, 15, 0, 0, 8, 15, LineTex, 0xFF, 1);
            BandY += 0x12;
        }
        Device->DrawText(X, Y, 0xFF808080, SSQSealStateName(GConsole, NULL, Seal), (INT)Width, 0, 1, L2FONT_Normal, 0,
                         0, 0, 0, 0, 0, 0);
        if (Seal == 1)
        {
            for (INT j = 0, k = 0; k < 0xC0; j += 2, k += 0x18)
            {
                Device->DrawText(X, (CharSize.cy + 6) * j + StateSize.cy + Y + 0x18, 0xFF808080,
                                 *GL2GameData.SysString[941 + j], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                Device->DrawText(X, (CharSize.cy + 6) * (j + 1) + StateSize.cy + Y + 0x18, 0xFF808080, TEXT(":"), 0, 0,
                                 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                Device->DrawText(X + 4, (CharSize.cy + 6) * (j + 1) + StateSize.cy + Y + 0x18, 0xFFB09B79,
                                 *GL2GameData.SysString[942 + j], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            }
        }
        else if (Seal == 2)
        {
            for (INT j = 0, k = 0; k < 0x90; j += 2, k += 0x18)
            {
                Device->DrawText(X, (CharSize.cy + 6) * j + StateSize.cy + Y + 0x18, 0xFF808080,
                                 *GL2GameData.SysString[957 + j], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                Device->DrawText(X, (CharSize.cy + 6) * (j + 1) + StateSize.cy + Y + 0x18, 0xFF808080, TEXT(":"), 0, 0,
                                 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
                Device->DrawText(X + 4, (CharSize.cy + 6) * (j + 1) + StateSize.cy + Y + 0x18, 0xFFB09B79,
                                 *GL2GameData.SysString[958 + j], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            }
        }
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
                Used += DrawItem(Device, Item->Children(i), 2, Used + Y);
            else
                Used += DrawItem(Device, Item->Children(i), X, Used + Y);
        }
    }
    return Used;
}
NW_HOOK_METHOD(0x100fe660, NCSSQSealStatusGaraTree, DrawItem)

NW_CHECK_VTABLE(NCSSQSealStatusGaraTree, 0x101ca380, 79)
