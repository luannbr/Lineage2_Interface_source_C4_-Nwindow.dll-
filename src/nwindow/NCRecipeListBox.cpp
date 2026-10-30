/*=============================================================================
    NCRecipeListBox.cpp - the recipe list of the dwarf's shop (new in C2).
    Stock (C3): 0x100DEAE0 (OnCreate), 0x100DF3C0 (OnPaint), 0x100DF8D0 (GetSelectedEntry), 0x100E09F0 (AddEntry),
    0x100E1E40 .. 0x100E1F00 (constructors, destructor), 0x1005FF80 (NFormatThousands).
    WindowProc (0x101241C0), OnDestroy (0x100E0980), OnLButtonDown (0x100862C0), HitTest (0x100DEB10) and Clear (0x100963E0)
    are bodies shared with the chat, train and henna list boxes (hooked there; stubs here).
=============================================================================*/
#include "nwindow/NCRecipeListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCHennaListBox.h"
#include "nwindow/NCTrainListBox.h"

#include <wchar.h>

// the stock code calls the CRT's wcscpy (0x1014c9bb); the intrinsic would copy inline
#pragma function(wcscpy)

enum { RECIPE_ROW_HEIGHT = 46 };   // a row is this plus LineGap high (C3; C2 43)
enum
{
    STR_Price = 641,    // sysstring (GL2GameData + 0x67898): the price line's label, in yellow
    STR_Adena = 469,    // (+0x67088) after the amount
    STR_Rate = 642,     // (+0x678A4): the success rate line's label, in grey
};

// 0x1006b4f0 (stdcall, ret 8) - Value's decimal digits with a comma between thousands (a sign is counted as a digit, so a
// negative amount of 4 characters gets its comma in front of the sign); the stock code builds the digits in a local buffer
// first
void __stdcall NFormatThousands(TCHAR* Out, INT Value)
{
    TCHAR Digits[128];
    appStrcpy(Digits, appItoa(Value));
    INT Len = (INT)wcslen(Digits);
    INT Index = Len - 1;
    INT OutLen = Index / 3 + Len;
    Out[OutLen] = 0;
    INT Counter = 0;
    TCHAR* P = Out + OutLen - 1;
    while (Index >= 0)
    {
        if (Counter % 3 == 0 && Index < (INT)wcslen(Digits) - 1)
        {
            *P = ',';
            P--;
        }
        *P = Digits[Index];
        Index--;
        Counter++;
        P--;
    }
}
NW_HOOK_FUNC(0x1006b4f0, NFormatThousands)

// 0x100300d0 (stdcall, ret 4; NCEditBox::OnPaint calls it too) - the color an amount's text is drawn in by the number of
// its characters that are not commas: light grey (C3: 0xFFDCDCDC, C2 white) up to four, then by (count - 2) % 4:
// 0x10034de0 (6, 10, ..), yellow (7), green (8), cyan (5, 9)
DWORD __stdcall NAmountColor(const TCHAR* Text)
{
    INT Len = appStrlen(Text);
    INT Commas = 0;
    for (INT i = 0; i < Len; i++)
    {
        if (Text[i] == ',')
            Commas++;
    }
    INT Digits = Len - Commas;
    if (Digits < 5)
        return 0xFFDCDCDC;
    switch ((Digits - 2) % 4)
    {
    case 0:
        return 0xFFFF80FF;
    case 1:
        return 0xFFFFFF00;
    case 2:
        return 0xFF00FF00;
    case 3:
        return 0xFF00FFFF;
    }
    return 0xFFDCDCDC;
}
NW_HOOK_FUNC(0x10034de0, NAmountColor)

// 0x100f30d0 - NCListBox's constructor, this class's vtable, no rows, the lines 4 apart
NCRecipeListBox::NCRecipeListBox()
{
    LineGap = 4;
}
NW_HOOK_CTOR(0x100f30d0, NCRecipeListBox)

// 0x100f3190 (ret 0xc) - the same with the list box's three numbers
NCRecipeListBox::NCRecipeListBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
    LineGap = 4;
}
NW_HOOK_CTOR_ARGS(0x100f3190, NCRecipeListBox, (INT A, INT B, INT C), (A, B, C))

// 0x100f3130
NW_HOOK_DTOR(0x100f3130, NCRecipeListBox)

// the stock bodies shared with other list boxes (hooked there): the slots of this list box enter them
// (the standalone gives each slot a body that runs the shared one: the stock vtable of the class lists those functions' addresses)
// 0x101241c0 (vslot 25, ret 0xC): NCChatListBox's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10020890)
#else
INT NCRecipeListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;   // 0x10020890: NCTextListBox::WindowProc, the plain forward
}
#endif

// 0x1001e850 (vslot 44): NCListBox's (NCWnd::ShowWindow, then the slider's range); NCListBox.h does not declare it
#if NW_HOOK_MODE
NW_NAKED void NCRecipeListBox::ShowWindow() NW_STOCK(0x10020880)
#else
// standalone: the stock slot 48 of this class holds NCListBox::ShowWindow (0x10020880) itself, so the override is an alias of it
NW_SLOT_IS("?ShowWindow@NCRecipeListBox@@UAEXXZ", "?ShowWindow@NCListBox@@UAEXXZ")   // NCRecipeListBox::ShowWindow = NCListBox::ShowWindow (0x10020880)
#endif

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100efcf0 (vslot 37) - the list box's, then the texture of the highlight under the selected row
INT NCRecipeListBox::OnCreate()
{
    NCListBox::OnCreate();
    HighlightTex = FindTexture(TEXT("L2UI_CH3.etc.IconSelect2"));
    return 1;
}
NW_HOOK_METHOD(0x100efcf0, NCRecipeListBox, OnCreate)

// 0x100e0980 (vslot 38): NCTrainListBox's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeListBox::OnDestroy() NW_STOCK(0x1011ddc0)
#else
INT NCRecipeListBox::OnDestroy()
{
    // 0x1011ddc0: one stock body for both lists (the array of rows is at +0x240 in both)
    return ((NCTrainListBox*)this)->NCTrainListBox::OnDestroy();
}
#endif

// 0x100862c0 (vslot 62, ret 8): NCTrainListBox's
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeListBox::OnLButtonDown(WPARAM wParam, LPARAM lParam) NW_STOCK(0x1011b870)
#else
INT NCRecipeListBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    return ((NCTrainListBox*)this)->NCTrainListBox::OnLButtonDown(wParam, lParam);   // 0x1011b870
}
#endif

// 0x100deb10 (vslot 69, ret 8): the henna list's; the row under the point, -1 outside the box (rows of 43 pixels and the gap)
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeListBox::HitTest(INT X, INT Y) NW_STOCK(0x10091680)
#else
INT NCRecipeListBox::HitTest(INT X, INT Y)
{
    return ((NCHennaListBox*)this)->NCHennaListBox::HitTest(X, Y);   // 0x10091680
}
#endif

// 0x100963e0 (vslot 71): the henna list's
#if NW_HOOK_MODE
NW_NAKED void NCRecipeListBox::Clear() NW_STOCK(0x100f1b60)// C3: no address for the base's 0x100963e0
#else
void NCRecipeListBox::Clear()
{
    ((NCTrainListBox*)this)->NCTrainListBox::Clear();   // 0x100f1b60: the rows freed, the array emptied, the slider reset
}
#endif

// 0x10086300  NOT PORTED (not in eligible.csv (category D or not a game function))
#if NW_HOOK_MODE
NW_NAKED void NCRecipeListBox::UpdateEntryScrollBar() NW_STOCK(0x10091760)
#else
void NCRecipeListBox::UpdateEntryScrollBar()
{
    ((NCHennaListBox*)this)->NCHennaListBox::UpdateEntryScrollBar();   // 0x10091760
}
#endif

// 0x100f0aa0 - the selected row's texts and numbers copied into Out (the texts and the icon only when the row has
// them); 0 with no row selected
__declspec(noinline) UBOOL NCRecipeListBox::GetSelectedEntry(NRecipeRow* Out)
{
    if (Selected < 0 || Selected >= Entries.Num())
        return 0;
    NRecipeRow* Row = Entries(Selected);
    if (!Row)
        return 0;
    if (Row->Name[0])
        wcscpy(Out->Name, Row->Name);
    if (Row->IconName[0])
        wcscpy(Out->IconName, Row->IconName);
    if (Row->Description[0])
        wcscpy(Out->Description, Row->Description);
    if (Row->Icon)
        Out->Icon = Row->Icon;
    Out->RecipeID = Row->RecipeID;
    Out->Unknown0A08 = Row->Unknown0A08;
    Out->SuccessRate = Row->SuccessRate;
    Out->Price = Row->Price;
    Out->Unknown0A18 = Row->Unknown0A18;
    return 1;
}
NW_HOOK_METHOD(0x100f0aa0, NCRecipeListBox, GetSelectedEntry)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100f1be0 - a new row, a copy of Src (the texts only when Src has them, the icon found by its name, or
// "NWindow.BlackTexture"); the list counts it (up to MaxLines; scrolled when it keeps the last row in view)
__declspec(noinline) void NCRecipeListBox::AddEntry(const NRecipeRow* Src)
{
    NRecipeRow* Row = (NRecipeRow*)appMalloc(sizeof(NRecipeRow), TEXT("new"));
    if (Row)
    {
        Row->Name[0] = 0;
        Row->IconName[0] = 0;
        Row->Description[0] = 0;
        Row->Icon = NULL;
        Row->RecipeID = -1;
        Row->Unknown0A08 = -1;
        Row->SuccessRate = 0;
        Row->Unknown0A14 = 0;
        Row->Unknown0A18 = 0;
        Row->Unknown0A1C = -1;
    }
    if (Src->Name[0])
        wcscpy(Row->Name, Src->Name);
    if (Src->IconName[0])
        wcscpy(Row->IconName, Src->IconName);
    if (Src->Description[0])
        wcscpy(Row->Description, Src->Description);
    Row->RecipeID = Src->RecipeID;
    Row->Unknown0A08 = Src->Unknown0A08;
    Row->Price = Src->Price;
    Row->Unknown0A18 = Src->Unknown0A18;
    Row->SuccessRate = Src->SuccessRate;
    Row->Icon = FindTexture(Row->IconName);
    if (!Row->Icon)
        Row->Icon = FindTexture(TEXT("NWindow.BlackTexture"));
    if (NumLines < MaxLines)
    {
        NumLines++;
        if (bAutoScroll && NumLines > VisibleLines)
            TopIndex++;
    }
    Entries.AddItem(Row);
    UpdateEntryScrollBar();
}
NW_HOOK_METHOD(0x100f1be0, NCRecipeListBox, AddEntry)

// 0x100f0590 (vslot 62) - the visible rows: the highlight under the selected one, the icon, the name, then the price
// line ("<label> : <amount> <Adena>", the amount colored by its size) and the success rate line ("<label> : <n>%")
// (the rows are 46 pixels, the texts lighter)
INT NCRecipeListBox::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeListBox::OnPaint);
    TCHAR Rate[128];
    TCHAR Amount[132];
    SIZE Size;
    INT Row = 0;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Entries.Num(); i++)
    {
        NRecipeRow* R = Entries(i);
        INT RowY = (LineGap + RECIPE_ROW_HEIGHT) * Row + TextY;
        if (i == Selected && HighlightTex)
            Device->DrawStretchedTex(0, RowY, (INT)(Width - 15.0f - 2.0f), RECIPE_ROW_HEIGHT, 0, 0, 32, 40, HighlightTex, 0xFF);
        if (R)
        {
            if (R->Icon)
                Device->DrawTexture(TextX, RowY + 7, 32, 32, 0, 0, 32, 32, R->Icon, 0xFF, 1);
            if (R->Name[0])
                Device->DrawText(TextX + 42, RowY + 3, 0xFFDCDCDC, R->Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        // the price line
        NCGDevice::GetTextExtent(&Size, *SysString(STR_Price), 0, 0, 0, 0, 0);
        Device->DrawText(TextX + 42, RowY + 17, 0xFFFFFF00, *SysString(STR_Price), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        INT ColonW = Device->DrawText(TextX + 42 + Size.cx, RowY + 17, 0xFFDCDCDC, TEXT(" : "), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NFormatThousands(Amount, R->Price);
        DWORD Color = NAmountColor(Amount);
        Device->DrawText(TextX + Size.cx + 42 + ColonW, RowY + 17, Color, Amount, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        INT LabelW = Size.cx;
        NCGDevice::GetTextExtent(&Size, Amount, 0, 0, 0, 0, 0);
        Device->DrawText(TextX + LabelW + Size.cx + 47 + ColonW, RowY + 17, 0xFFFFFF00, *SysString(STR_Adena), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        // the success rate line
        NCGDevice::GetTextExtent(&Size, *SysString(STR_Rate), 0, 0, 0, 0, 0);
        Device->DrawText(TextX + 42, RowY + 31, 0xFFA3A3A3, *SysString(STR_Rate), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        ColonW = Device->DrawText(TextX + 42 + Size.cx, RowY + 31, 0xFFDCDCDC, TEXT(" : "), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        appSprintf(Rate, TEXT("%d%%"), R->SuccessRate);
        Device->DrawText(TextX + ColonW + 42 + Size.cx, RowY + 31, 0xFFB09B79, Rate, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        Row++;
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f0590, NCRecipeListBox, OnPaint)

NW_CHECK_VTABLE(NCRecipeListBox, 0x101c7318, 82)
