#pragma once
// Stand-ins for the small anonymous methods the stock keeps for the console (names ours; the bodies are in NLeftHelpers.cpp and
// NTemplates.cpp, hooked at the C4 addresses below). `this` is the console, or is not read at all by the stock bodies, so the
// windows reach them through a cast of GConsole:
//     ((NConsoleLeftFns*)GConsole)->GetClassGroup(ClassID)
// Only the methods the windows of lane sa-win-b call are declared here (the structs add no data and no virtuals, so the
// declarations of the two .cpp files and this one agree on the layout and on the mangled names).

#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleFns.h"   // NConsoleLeftFns / NConsoleForwardFns: defined once there (stage 5, sa-verify)
#include "nwindow/NCComboBox.h"
#include "nwindow/NCConsole.h"

// 0x10030670 (ret 0xC) - NToolTipInfo's list setter: the tip made of lines "label" with an icon each (the quest windows' OnCreate
// is a caller). The method is defined on this view of NToolTipInfo in NCGMWnd.cpp (the tool tip's header does not declare it)
struct NToolTipInfoLinesView : public NToolTipInfo
{
    __declspec(noinline) void SetIconLines(TArray<FString*>* Texts, TArray<UTexture*>* Icons, DWORD InStyle);
};

// 0x10074c70 (a stdcall function of NConsoleWndAnon.cpp, static there; ret 4) - the icon of a party member's class by the class's
// group (the console's GetClassGroup), "party_styleicon" 1 .. 7, 1_1 .. 1_3 and 2_3 .. 7_3; NULL for another group. The standalone
// windows have this copy of the table (the hook build calls the stock function)
FORCEINLINE const TCHAR* NPartyStyleIconName(INT ClassID)
{
    switch (((NConsoleLeftFns*)GConsole)->GetClassGroup(ClassID))
    {
    case 0: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1");
    case 1: return TEXT("L2UI_CH3.PartyWnd.party_styleicon2");
    case 2: return TEXT("L2UI_CH3.PartyWnd.party_styleicon3");
    case 3: return TEXT("L2UI_CH3.PartyWnd.party_styleicon4");
    case 4: return TEXT("L2UI_CH3.PartyWnd.party_styleicon5");
    case 5: return TEXT("L2UI_CH3.PartyWnd.party_styleicon6");
    case 6: return TEXT("L2UI_CH3.PartyWnd.party_styleicon7");
    case 7: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_1");
    case 8: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_2");
    case 9: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_3");
    case 10: return TEXT("L2UI_CH3.PartyWnd.party_styleicon2_3");
    case 11: return TEXT("L2UI_CH3.PartyWnd.party_styleicon3_3");
    case 12: return TEXT("L2UI_CH3.PartyWnd.party_styleicon4_3");
    case 13: return TEXT("L2UI_CH3.PartyWnd.party_styleicon5_3");
    case 14: return TEXT("L2UI_CH3.PartyWnd.party_styleicon6_3");
    case 15: return TEXT("L2UI_CH3.PartyWnd.party_styleicon7_3");
    }
    return NULL;
}

// 0x10006410 (ret 4; NTemplates.cpp's NComboItemFns::GetItemData) - the data dword of a combo box's record, -1 outside the list
FORCEINLINE INT NComboItemData(NCComboBox* Combo, INT Index)
{
    if (Index < 0 || Index >= Combo->Items.Num())
        return -1;
    return Combo->Items(Index).Data;
}
