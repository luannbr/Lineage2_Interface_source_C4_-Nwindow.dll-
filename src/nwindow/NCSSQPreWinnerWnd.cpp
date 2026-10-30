/*=============================================================================
    NCSSQPreWinnerWnd.cpp - the previous winners list of the Seven Signs status page. C3 stock: 0x100E91A0 (paint),
    0x100E9940 / 0x100E9A00 (constructor, destructor).
=============================================================================*/
#include "nwindow/NCSSQPreWinnerWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NSSQHelpers.h"

// 0x100fc1d0 - the three arrays start empty and the band texture is found
NCSSQPreWinnerWnd::NCSSQPreWinnerWnd()
{
    LineTex = FindTexture(TEXT("L2UI_CH3.Etc.TextBackline"));
}
NW_HOOK_CTOR(0x100fc1d0, NCSSQPreWinnerWnd)

// 0x100fc290 - the three arrays go (TIntArray's destructor), then NCScrollWnd's
NW_HOOK_DTOR(0x100fc290, NCSSQPreWinnerWnd)

// 0x100fb930 (vslot 56) - a block per seal, its name and the cabal that won (or "-"), then the message under the
// name; every second 17 pixel band of the whole height is drawn on the band texture. The content height is the
// total of the blocks
INT NCSSQPreWinnerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSSQPreWinnerWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->PushOrigin(ScrollX, ScrollY);
    INT Total = 0;
    TCHAR NameText[128];
    SIZE NameSize;
    SIZE MessageSize;
    for (INT i = 0; i < Seals.Num(); i++)
    {
        INT Seal = Seals(i);
        INT Winner = Winners(i);
        appSprintf(NameText, TEXT("%s : "), SSQSealName(GConsole, NULL, Seal));
        NCGDevice::GetTextExtent(&NameSize, NameText, 0, 0, 0, 0, 0);
        if (i >= 1)
        {
            const TCHAR* Message = *GL2GameData.SystemMsg[Messages(i - 1)].Message;
            NCGDevice::GetTextExtent(&MessageSize, Message, (INT)(Width - 26.0f), 0, 1, 0, 0);
        }
        else
        {
            MessageSize.cy = 0;
        }
        Device->DrawText(6, (MessageSize.cy + NameSize.cy * 2 + 15) * i + 3, 0xFF808080, NameText, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        const TCHAR* Own = *GL2GameData.SystemMsg[Messages(i)].Message;
        Device->DrawText(10, (MessageSize.cy + NameSize.cy * 2 + 15) * i + NameSize.cy + 9, 0xFF808080, Own,
                         (INT)(Width - 26.0f), 0, 1, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        if (Winner == 1)
            Device->DrawText(NameSize.cx + 6, (MessageSize.cy + NameSize.cy * 2 + 15) * i + 3, 0xFFB09B79,
                             SSQCabalName(GConsole, NULL, Winner), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else if (Winner == 2)
            Device->DrawText(NameSize.cx + 6, (MessageSize.cy + NameSize.cy * 2 + 15) * i + 3, 0xFFB09B79,
                             SSQCabalName(GConsole, NULL, Winner), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else
            Device->DrawText(NameSize.cx + 6, (MessageSize.cy + NameSize.cy * 2 + 15) * i + 3, 0xFFB09B79,
                             *GL2GameData.SysString[936], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Total += MessageSize.cy + NameSize.cy * 2 + 17;
    }
    for (INT i = 0; i < Total / 16; i++)
    {
        if (i % 2 == 1)
            Device->DrawTexture(0, i * 17 + 2, (INT)Width, 16, 0, 0, 8, 15, LineTex, 0xFF, 1);
    }
    SetContentHeight(Total);
    Device->PopOrigin();
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fb930, NCSSQPreWinnerWnd, OnPaint)

NW_CHECK_VTABLE(NCSSQPreWinnerWnd, 0x101c95a8, 78)
