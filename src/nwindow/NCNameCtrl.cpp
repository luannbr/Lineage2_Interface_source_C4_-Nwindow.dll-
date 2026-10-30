/*=============================================================================
    NCNameCtrl.cpp - the name control of the olympiad windows (C4 only). Stock: 0x1002d260 (constructor), 0x1002d310 (destructor),
    0x1002ce20 (OnPaint), 0x1002d390 (OnDrawToolTip), 0x1002cb30 / 0x1002c7c0 (the two painters).
=============================================================================*/
#include "nwindow/NCNameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x1002d260 - NControl's fields, the name and the tool tip empty, the tool tip on
NCNameCtrl::NCNameCtrl()
{
    bToolTip = 1;
}
NW_HOOK_CTOR(0x1002d260, NCNameCtrl)

// 0x1002d310
NW_HOOK_DTOR(0x1002d310, NCNameCtrl)

// 0x1002ce20 (vslot 62, ret 4) - clipped to the control and to its parent (the name never leaves the parent), painted by the painter
// of the Kind
INT NCNameCtrl::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    NCWnd* Parent = Entry->Parent->Wnd;
    Device->SetClip(Parent->ScreenX, Parent->ScreenY, (INT)Parent->Width, (INT)Parent->Height);
    if (Kind == 0)
        PaintName(Device);
    else if (Kind == 1)
        PaintItemName(Device);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x1002ce20, NCNameCtrl, OnPaint)

// 0x1002cb30 (ret 4; anonymous in the db) - the name cut with ".." to the width, left aligned or centered
void NCNameCtrl::PaintName(NCGDevice* Device)
{
    TCHAR Text[256];
    SIZE Full;
    appStrcpy(Text, *Name);
    NCGDevice::GetTextExtent(&Full, Text, 0, 0, 0, 0, 0);
    INT Over = (INT)((FLOAT)Full.cx - Width);
    if (Over > 0)
    {
        SIZE Dots, Cut;
        NCGDevice::GetTextExtent(&Dots, TEXT(".."), 0, 0, 0, 0, 0);
        INT Len = appStrlen(Text);
        INT End;
        do
        {
            End = Len;
            Len = End - 1;
            if (Len < 0)
                break;
            Text[Len] = 0;
            NCGDevice::GetTextExtent(&Cut, Text, 0, 0, 0, 0, 0);
        } while ((Full.cx - Dots.cx) - Cut.cx <= Over);
        Text[Len] = '.';
        Text[End] = '.';
        Text[End + 1] = 0;
    }
    if (Align == 0)
    {
        Device->DrawText(0, 0, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    else if (Align == 1)
    {
        SIZE Now;
        NCGDevice::GetTextExtent(&Now, Text, 0, 0, 0, 0, 0);
        INT Half = Now.cx / 2;
        Device->DrawText((INT)(Width * 0.5f - Half), 0, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
}
NW_HOOK_METHOD(0x1002cb30, NCNameCtrl, PaintName)

// 0x1002c7c0 (ret 4; anonymous in the db) - an item's name: "+N name" (the enchant of a weapon, armor or jewel), then its additional name
// (yellow) and the grade (between quotes) and the amount (xN) after it, each starting where the one before ends; when the whole is
// wider than the control the name is cut with ".." and the rest moves left by what was cut
void NCNameCtrl::PaintItemName(NCGDevice* Device)
{
    TCHAR Raw[256], Full[256], Additional[256], GradeText[256], CountText[256];
    INT ItemNumber = ItemID;
    appStrcpy(Raw, *Name);
    INT GradeIndex = Grade;
    INT Amount = Count;
    INT Type = ItemType;
    INT Plus = Enchant;
    INT ControlW = (INT)Width;
    if (Plus > 0 && (Type == 0 || Type == 1 || Type == 3 || Type == 2))
        appSprintf(Full, TEXT("+%d %s"), Plus, Raw);
    else
        appSprintf(Full, TEXT("%s"), Raw);
    SIZE FullSize;
    NCGDevice::GetTextExtent(&FullSize, Full, 0, 0, 0, 0, 0);
    UBOOL bAdditional = GConsole->HasItemAdditionalName(ItemNumber);
    SIZE AdditionalSize;
    if (bAdditional)
    {
        appSprintf(Additional, TEXT("%s"), GConsole->GetItemAdditionalName(ItemNumber));
        NCGDevice::GetTextExtent(&AdditionalSize, Additional, 0, 0, 0, 0, 0);
    }
    UBOOL bGrade = 0;
    INT EndOfName = FullSize.cx;
    if (bAdditional)
        EndOfName = AdditionalSize.cx + 2 + FullSize.cx;
    INT EndOfAdditional = EndOfName;
    if (GradeIndex >= 0 && GradeIndex < 5 && GConsole->GetGradeText(GradeIndex))
    {
        appSprintf(GradeText, TEXT("`%s`"), GConsole->GetGradeText(GradeIndex));
        bGrade = 1;
        EndOfAdditional = EndOfName + 14;
    }
    UBOOL bCount = Amount > 0;
    INT Total = EndOfAdditional;
    if (bCount)
    {
        appSprintf(CountText, TEXT("x%d"), Amount);
        SIZE CountSize;
        NCGDevice::GetTextExtent(&CountSize, CountText, 0, 0, 0, 0, 0);
        Total = CountSize.cx + 2 + EndOfAdditional;
    }
    INT Over = Total - ControlW;
    INT Cut = 0;
    if (Over > 0)
    {
        SIZE Dots, Now;
        NCGDevice::GetTextExtent(&Dots, TEXT(".."), 0, 0, 0, 0, 0);
        INT Len = appStrlen(Full);
        INT End;
        do
        {
            End = Len;
            Len = End - 1;
            if (Len < 0)
                break;
            Full[Len] = 0;
            NCGDevice::GetTextExtent(&Now, Full, 0, 0, 0, 0, 0);
        } while ((FullSize.cx - Dots.cx) - Now.cx <= Over);
        Full[Len] = '.';
        Full[End] = '.';
        Cut = (FullSize.cx - Dots.cx) - Now.cx;
        Full[End + 1] = 0;
    }
    Device->DrawText(0, 0, 0xFFDCDCDC, Full, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (bAdditional)
        Device->DrawText((FullSize.cx - Cut) + 2, 0, 0xFFFFD969, Additional, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (bGrade)
        Device->DrawText((EndOfName - Cut) + 2, 0, 0xFFDCDCDC, GradeText, 0, 0, 0, L2FONT_Normal, 1, 0, 0, 0, 0, 0, 0);
    if (bCount)
        Device->DrawText((EndOfAdditional - Cut) + 2, 0, 0xFFDCDCDC, CountText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
}
NW_HOOK_METHOD(0x1002c7c0, NCNameCtrl, PaintItemName)

// 0x1002d390 (vslot 63, ret 4) - the tool tip is the window skin's, as the buttons' (the desktop's local rectangle is asked for and
// not used)
INT NCNameCtrl::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bToolTip)
    {
        NRectF Screen;
        Screen.X = 0;
        Screen.Y = 0;
        Screen.W = 0.0f;
        Screen.H = 0.0f;
        GConsole->Desktop->GetLocalRect(&Screen);
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            ToolTipSkin->Draw(Device, ToolTip, 0, 0, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x1002d390, NCNameCtrl, OnDrawToolTip)

NW_CHECK_VTABLE(NCNameCtrl, 0x101a6668, 75)
