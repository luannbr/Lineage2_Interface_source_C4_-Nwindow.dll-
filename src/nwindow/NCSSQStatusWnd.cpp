/*=============================================================================
    NCSSQStatusWnd.cpp - the Seven Signs status page. C3 stock: 0x100E7E10 (WindowProc), 0x100E7E90 .. 0x100E8BDF
    (PaintCurrent, 3395 bytes), 0x100E8BE0 (PaintPrevious), 0x100E8F00 .. 0x100E901C (ResetStatus, the two commands,
    ShowWindow), 0x100E9550 (paint), 0x100E9A80 .. 0x100E9E00 (constructor, destructor, OnCreate).
=============================================================================*/
#include "nwindow/NCSSQStatusWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSSQPreWinnerWnd.h"
#include "nwindow/NSSQHelpers.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_SSQSTATUS_PAGE = 0xFF0F0F11,
    NID_SSQSTATUS_REFRESH = 0xFF0F0F12,
};

// sysstring lines of the buttons (names ours)
enum
{
    STR_Previous = 0x3A9,            // the button while the current page is shown
    STR_Current = 0x3AB,             // the button while the previous period's page is shown
    STR_Refresh = 0x3AA,
};

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100fc310 - the textures; the page starts on the current period's
NCSSQStatusWnd::NCSSQStatusWnd()
{
    BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.SSQ_back1"));
    BackTex2 = FindTexture(TEXT("L2UI_CH3.SSQWnd.SSQ_back1_1"));
    Bar22Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar22"));
    Bar12Tex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar12"));
    Bar2BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar2back"));
    Bar1BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_bar1back"));
    TextBox1Tex = FindTexture(TEXT("L2UI_CH3.Etc.TextBox1"));
    TextBox2Tex = FindTexture(TEXT("L2UI_CH3.Etc.TextBox2"));
    TextBox3Tex = FindTexture(TEXT("L2UI_CH3.Etc.TextBox3"));
    bShowPrevious = 0;
}
NW_HOOK_CTOR(0x100fc310, NCSSQStatusWnd)

// 0x100fc440
NW_HOOK_DTOR(0x100fc440, NCSSQStatusWnd)

// 0x100fbce0 (slot 38, name ours: OnDestroyBody) - guarded; the winners list, the page button and the refresh button go (the
// textures are not ours), then NCWnd's
INT NCSSQStatusWnd::OnDestroyBody()
{
    guard(NCSSQStatusWnd::OnDestroy);
    if (PreWinnerWnd)
    {
        RemoveChildWindow(PreWinnerWnd->Entry);
        delete PreWinnerWnd;
        PreWinnerWnd = NULL;
    }
    if (PageButton)
    {
        RemoveChildWindow(PageButton->Entry);
        delete PageButton;
        PageButton = NULL;
    }
    if (RefreshButton)
    {
        RemoveChildWindow(RefreshButton->Entry);
        delete RefreshButton;
        RefreshButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fbce0, NCSSQStatusWnd, OnDestroyBody)

// 0x100fa5a0 (vslot 25) - the two buttons
INT NCSSQStatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_SSQSTATUS_PAGE)
            return OnTogglePage() != 0;
        if (wParam == NID_SSQSTATUS_REFRESH)
            return OnRefresh() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100fa5a0, NCSSQStatusWnd, WindowProc)

// 0x100fb6f0 (vslot 68, name ours) - the other page: the console is asked for the page and the winners list shows or
// hides (the stock sets the button text 0x3AB in both directions)
INT NCSSQStatusWnd::OnTogglePage()
{
    bShowPrevious = (bShowPrevious == 0);
    if (bShowPrevious)
    {
        PageButton->SetTextID(STR_Current);
        ((NConsoleSSQ*)GConsole)->RequestSSQStatus(4);
        PreWinnerWnd->ShowWindow();
        return 1;
    }
    PageButton->SetTextID(STR_Current);   // the stock pushes 0x3AB once for both branches
    ((NConsoleSSQ*)GConsole)->RequestSSQStatus(1);
    PreWinnerWnd->HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100fb6f0, NCSSQStatusWnd, OnTogglePage)

// 0x100fb770 (vslot 69, name ours) - the same page asked for again
INT NCSSQStatusWnd::OnRefresh()
{
    if (bShowPrevious)
    {
        ((NConsoleSSQ*)GConsole)->RequestSSQStatus(4);
        return 1;
    }
    ((NConsoleSSQ*)GConsole)->RequestSSQStatus(1);
    PreWinnerWnd->HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100fb770, NCSSQStatusWnd, OnRefresh)

// 0x100fb7b0 (vslot 48) - the button and the winners list follow the page
void NCSSQStatusWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (bShowPrevious)
    {
        PageButton->SetTextID(STR_Current);
        PreWinnerWnd->ShowWindow();
    }
    else
    {
        PageButton->SetTextID(STR_Previous);
        PreWinnerWnd->HideWindow();
    }
}
NW_HOOK_METHOD(0x100fb7b0, NCSSQStatusWnd, ShowWindow)

// 0x100fb690 (name ours) - the numbers cleared and the current period's page again (the main board hides the window)
void NCSSQStatusWnd::ResetStatus()
{
    Unknown0C8 = 0;
    Cabal = 0;
    Seal = 0;
    Number = 0;
    UnknownD8 = 0;
    UnknownDC = 0;
    UnknownE0 = 0;
    UnknownE4 = 0;
    bShowPrevious = 0;
    PageButton->SetTextID(STR_Previous);
    PreWinnerWnd->HideWindow();
}
NW_HOOK_METHOD(0x100fb690, NCSSQStatusWnd, ResetStatus)

// 0x100fbde0 (vslot 62) - one of the two pages
INT NCSSQStatusWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSSQStatusWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bShowPrevious)
        PaintPrevious(Device);
    else
        PaintCurrent(Device);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fbde0, NCSSQStatusWnd, OnPaint)

// the number boxes of the current page: a three part frame (8 + 63 + 8 pixels wide, 17 high) at (X, Y); the number is
// right aligned in it (static helper, inlined in the stock code)
#define SSQ_NUMBER_BOX(Tex1, Tex2, Tex3, X, Y)                                                                   \
    Device->DrawTexture((X), (Y), 8, 0x11, 0, 0, 8, 0x11, (Tex1), 0xFF, 1);                                       \
    Device->DrawTexture((X) + 8, (Y), 0x3F, 0x11, 0, 0, 8, 0x11, (Tex2), 0xFF, 1);                                \
    Device->DrawTexture((X) + 0x47, (Y), 8, 0x11, 0, 0, 8, 0x11, (Tex3), 0xFF, 1);

// 0x100fa620 (name ours, ret 4) - the current period's page on the first panel: the title ("<sysstring 833> - " and
// "<number> <sysstring 934>"), the system messages (the status window's lines), the cabal that leads and the seal,
// the numbers of the period and the two share bars of the cabals (the second's, then the first's, 1.5 pixels per
// percent on the 150 pixel backs) and the five number boxes with their labels
void NCSSQStatusWnd::PaintCurrent(NCGDevice* Device)
{
    TCHAR TitleText[128];
    TCHAR NumberText[128];
    TCHAR BoxText3[128];
    TCHAR BoxText4[128];
    TCHAR BoxText5[128];
    TCHAR EndText[128];
    TCHAR BoxText6[128];
    TCHAR BoxText2[128];
    TCHAR BoxText7[128];
    INT H = (INT)Height;
    INT W = (INT)Width;
    Device->DrawTexture(0, 0, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    SIZE CharSize;
    NCGDevice::GetCharSize(&CharSize, 'A', 0);
    appSprintf(TitleText, TEXT("%s - "), *GL2GameData.SysString[833]);
    SIZE TitleSize;
    NCGDevice::GetTextExtent(&TitleSize, TitleText, 0, 0, 0, 0, 0);
    Device->DrawText(11, 13, 0xFFDCDCDC, TitleText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appSprintf(TitleText, TEXT("%d %s"), UnknownF8, *GL2GameData.SysString[934]);
    Device->DrawText(TitleSize.cx + 11, 13, 0xFFB09B79, TitleText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (UnknownFC > 0)
        Device->DrawText(14, 34, 0xFF808080, *GL2GameData.SystemMsg[UnknownFC].Message, 0, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0, 0);
    if (Unknown100 > 0)
        Device->DrawText(14, TitleSize.cy + 38, 0xFFB09B79, *GL2GameData.SystemMsg[Unknown100].Message, 0, 0, 0,
                         L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 73, 0xFFDCDCDC, *GL2GameData.SysString[834], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(14, 94, 0xFF808080, *GL2GameData.SysString[835], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(100, 94, 0xFFB09B79, SSQCabalName(GConsole, NULL, Cabal), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(14, CharSize.cy + 98, 0xFF808080, *GL2GameData.SysString[836], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(100, CharSize.cy + 98, 0xFFB09B79, SSQSealName(GConsole, NULL, Seal), 0, 0, 0, L2FONT_Normal, 0,
                     0, 0, 0, 0, 0, 0);
    Device->DrawText(14, CharSize.cy * 2 + 102, 0xFF808080, *GL2GameData.SysString[931], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    appSprintf(NumberText, TEXT("%d%s"), Number, *GL2GameData.SysString[932]);
    Device->DrawText(100, CharSize.cy * 2 + 102, 0xFFB09B79, NumberText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appSprintf(NumberText, TEXT("(%d%s)"), Unknown104, *GL2GameData.SysString[933]);
    Device->DrawText(100, CharSize.cy * 3 + 106, 0xFFB09B79, NumberText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 168, 0xFFDCDCDC, *GL2GameData.SysString[824], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    SIZE LineSize;
    NCGDevice::GetTextExtent(&LineSize, *GL2GameData.SysString[824], 0, 0, 0, 0, 0);
    if (Unknown0C8 == 3)
        appSprintf(EndText, TEXT(" - %s"), *GL2GameData.SysString[838]);
    else
        appSprintf(EndText, TEXT(" - %s"), *GL2GameData.SysString[837]);
    Device->DrawText(LineSize.cx + 11, 168, 0xFFB09B79, EndText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    FLOAT ShareFor2 = (FLOAT)UnknownEC;
    FLOAT ShareFor1 = (FLOAT)UnknownE8;
    Device->DrawText(14, 192, 0xFFB09B79, SSQCabalName(GConsole, NULL, 2), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                     0);
    Device->DrawText(14, 210, 0xFFB09B79, SSQCabalName(GConsole, NULL, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                     0);
    Device->DrawTexture(0x5C, 0xC0, 0x96, 0xB, 0, 0, 8, 0xB, Bar2BackTex, 0xFF, 1);
    Device->DrawTexture(0x5C, 0xC0, (INT)(ShareFor2 * 1.5f), 0xB, 0, 0, 8, 0xB, Bar22Tex, 0xFF, 1);
    Device->DrawTexture(0x5C, 0xD2, 0x96, 0xB, 0, 0, 8, 0xB, Bar1BackTex, 0xFF, 1);
    Device->DrawTexture(0x5C, 0xD2, (INT)(ShareFor1 * 1.5f), 0xB, 0, 0, 8, 0xB, Bar12Tex, 0xFF, 1);
    Device->DrawText(82, 237, 0xFF808080, SSQCabalName(GConsole, NULL, 2), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                     0);
    Device->DrawText(166, 237, 0xFF808080, SSQCabalName(GConsole, NULL, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                     0);
    Device->DrawText(14, 252, 0xFF808080, *GL2GameData.SysString[839], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(14, CharSize.cy + 260, 0xFF808080, *GL2GameData.SysString[825], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(14, CharSize.cy * 2 + 268, 0xFF808080, *GL2GameData.SysString[840], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    SIZE NumSize;
    // the first column of boxes (x 78): the three numbers of the cabal 2, right aligned at x 155
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0x4E, 0xFA)
    appSprintf(BoxText2, TEXT("%d"), UnknownDC);
    NCGDevice::GetTextExtent(&NumSize, BoxText2, 0, 0, 0, 0, 0);
    Device->DrawText(0x9B - NumSize.cx, 0x103 - NumSize.cy / 2, 0xFF808080, BoxText2, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0x4E, 0x10D)
    appSprintf(BoxText3, TEXT("%d"), UnknownE4);
    NCGDevice::GetTextExtent(&NumSize, BoxText3, 0, 0, 0, 0, 0);
    Device->DrawText(0x9B - NumSize.cx, 0x116 - NumSize.cy / 2, 0xFF808080, BoxText3, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0x4E, 0x120)
    appSprintf(BoxText4, TEXT("%d"), UnknownF4);
    NCGDevice::GetTextExtent(&NumSize, BoxText4, 0, 0, 0, 0, 0);
    Device->DrawText(0x9B - NumSize.cx, 0x129 - NumSize.cy / 2, 0xFF808080, BoxText4, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    // the second column (x 166): the cabal 1's, right aligned at x 243
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0xA6, 0xFA)
    appSprintf(BoxText5, TEXT("%d"), UnknownD8);
    NCGDevice::GetTextExtent(&NumSize, BoxText5, 0, 0, 0, 0, 0);
    Device->DrawText(0xF3 - NumSize.cx, 0x103 - NumSize.cy / 2, 0xFF808080, BoxText5, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0xA6, 0x10D)
    appSprintf(BoxText6, TEXT("%d"), UnknownE0);
    NCGDevice::GetTextExtent(&NumSize, BoxText6, 0, 0, 0, 0, 0);
    Device->DrawText(0xF3 - NumSize.cx, 0x116 - NumSize.cy / 2, 0xFF808080, BoxText6, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    SSQ_NUMBER_BOX(TextBox1Tex, TextBox2Tex, TextBox3Tex, 0xA6, 0x120)
    appSprintf(BoxText7, TEXT("%d"), UnknownF0);
    NCGDevice::GetTextExtent(&NumSize, BoxText7, 0, 0, 0, 0, 0);
    Device->DrawText(0xF3 - NumSize.cx, 0x129 - NumSize.cy / 2, 0xFF808080, BoxText7, 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
}
NW_HOOK_METHOD(0x100fa620, NCSSQStatusWnd, PaintCurrent)

// 0x100fb370 - the second panel, the title, the cabal that won ("<text> <cabal>" with the period's text), its name
// and the two lines of text under it
void NCSSQStatusWnd::PaintPrevious(NCGDevice* Device)
{
    Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex2, 0xFF, 1);
    Device->DrawText(11, 13, 0xFFDCDCDC, *GL2GameData.SysString[937], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    SIZE TitleSize;
    NCGDevice::GetTextExtent(&TitleSize, *GL2GameData.SysString[937], 0, 0, 0, 0, 0);
    TCHAR Line[128];
    Line[0] = 0;
    if (Mode == 1)
        appSprintf(Line, TEXT("%s %s"), SSQCabalName(GConsole, NULL, 1), *GL2GameData.SysString[828]);
    else if (Mode == 2)
        appSprintf(Line, TEXT("%s %s"), SSQCabalName(GConsole, NULL, 2), *GL2GameData.SysString[828]);
    Device->DrawText(TitleSize.cx + 16, 13, 0xFFB09B79, Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (Mode == 1)
        appSprintf(Line, TEXT("%s"), SSQCabalName(GConsole, NULL, Mode));
    else if (Mode == 2)
        appSprintf(Line, TEXT("%s"), SSQCabalName(GConsole, NULL, Mode));
    TCHAR Text[1024];
    if (Mode != 0)
        GConsole->FormatSystemMessage(Text, *GL2GameData.SystemMsg[1288].Message, Line, 0);
    else
        appStrcpy(Text, *GL2GameData.SystemMsg[1293].Message);
    Device->DrawText(14, 32, 0xFF808080, Text, (INT)(Width - 30.0f), 0, 1, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 88, 0xFFDCDCDC, *GL2GameData.SysString[940], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
}
NW_HOOK_METHOD(0x100fb370, NCSSQStatusWnd, PaintPrevious)

// 0x100fc490 (vslot 37) - the two buttons (76 x 23 at y 324) and the winners list under the title (240 x 212)
INT NCSSQStatusWnd::OnCreate()
{
    NCWnd::OnCreate();

    RefreshButton = new NCButton(TEXT("L2ui_ch3.button.Btn1_normal"), TEXT("L2ui_ch3.button.Btn1_normalOn"), 0);
    RefreshButton->ID = NID_SSQSTATUS_REFRESH;
    RefreshButton->SetPosition(51, 324, 76, 23);
    CreateChild(RefreshButton, NWS_VISIBLE);
    RefreshButton->SetTextID(STR_Refresh);

    PageButton = new NCButton(TEXT("L2ui_ch3.button.Btn1_normal"), TEXT("L2ui_ch3.button.Btn1_normalOn"), 0);
    PageButton->ID = NID_SSQSTATUS_PAGE;
    PageButton->SetPosition(131, 324, 76, 23);
    CreateChild(PageButton, NWS_VISIBLE);
    PageButton->SetTextID(STR_Previous);

    PreWinnerWnd = new NCSSQPreWinnerWnd();
    PreWinnerWnd->SetPosition(8, 103, 240, 212);
    CreateChild(PreWinnerWnd, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100fc490, NCSSQStatusWnd, OnCreate)

NW_CHECK_VTABLE(NCSSQStatusWnd, 0x101c9880, 76)
